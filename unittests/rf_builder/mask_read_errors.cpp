#include <catch2/catch_test_macros.hpp>
#include "vector_mask.h"

namespace {
class FailingLayer final : public OGRLayer {
public:
    FailingLayer()
    {
        m_definition.Reference();
        m_reference.importFromEPSG(3857);
        m_reference.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    void ResetReading() override { m_read = false; }
    OGRFeatureDefn* GetLayerDefn() override { return &m_definition; }
    OGRSpatialReference* GetSpatialRef() override { return &m_reference; }
    int TestCapability(const char*) override { return FALSE; }
    OGRFeature* GetNextFeature() override
    {
        if (m_read) {
            CPLError(CE_Failure, CPLE_AppDefined, "injected feature-read failure");
            return nullptr;
        }
        m_read = true;
        auto feature = std::make_unique<OGRFeature>(&m_definition);
        OGRLinearRing ring;
        ring.addPoint(0, 0);
        ring.addPoint(10, 0);
        ring.addPoint(10, 10);
        ring.addPoint(0, 10);
        ring.addPoint(0, 0);
        OGRPolygon polygon;
        REQUIRE(polygon.addRing(&ring) == OGRERR_NONE);
        REQUIRE(feature->SetGeometry(&polygon) == OGRERR_NONE);
        return feature.release();
    }
private:
    OGRFeatureDefn m_definition { "mask" };
    OGRSpatialReference m_reference;
    bool m_read = false;
};

class FailingDataset final : public GDALDataset {
public:
    int GetLayerCount() override { return 1; }
    OGRLayer* GetLayer(int index) override { return index == 0 ? &m_layer : nullptr; }
private:
    FailingLayer m_layer;
};
}

TEST_CASE("RF shared mask loading distinguishes a read failure from normal EOF", "[rf-builder]")
{
    Dataset dataset(new FailingDataset);
    auto loaded = vector_mask::load_referenced_from_dataset(dataset);
    REQUIRE_FALSE(loaded);
    CHECK(loaded.error() == vector_mask::LoadError(vector_mask::LoadErrorKind::ReadFailure));
}
