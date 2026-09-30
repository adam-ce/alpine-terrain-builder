/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
