#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "atlas/Packer.h"
#include "atlas/pull_reproject_texture.h"
#include "mesh/bounds.h"
#include "texture_sizing.h"
#include "opencv_utils.h"
#include "range_utils.h"
#include "TextureSet.h"

using Texture = cv::Mat;
using TextureMapId = uint32_t;

class PackedAtlas;

struct TextureMap {
    std::vector<glm::dvec2> uvs;
    Texture texture;
};

struct TexturedMesh {
    std::vector<glm::uvec3> triangles;
    TextureMap map;
};

struct MappedTriangle {
    size_t source_map;
    glm::uvec3 source;
    glm::uvec3 target;
};

struct TextureComposition {
    std::vector<TextureMap> maps;
    std::vector<MappedTriangle> triangles;
    std::vector<glm::dvec2> target_uvs;
};

namespace detail {
inline glm::uvec2 get_effective_texture_size(const TextureMap &map) {
    const radix::geometry::Aabb2d uv_bounds = calculate_bounds(map.uvs);
    const glm::dvec2 size = get_texture_size(map.texture);
    return glm::uvec2(glm::ceil(size * uv_bounds.size()));
}

inline double get_effective_pixel_area(const TextureComposition &comp) {
    std::vector<radix::geometry::Aabb2d> bounds(comp.maps.size());
    for (const MappedTriangle &triangle : comp.triangles) {
        const std::vector<glm::dvec2> &uvs = comp.maps[triangle.source_map].uvs;
        for (uint8_t k = 0; k < 3; k++) {
            bounds[triangle.source_map].expand_by(uvs[triangle.source[k]]);
        }
    }

    double area = 0;
    for (const auto &[map_index, map] : enumerate(comp.maps)) {
        area += glm::compMul(glm::dvec2(get_texture_size(map.texture)) * bounds[map_index].size());
    }
    return area;
}
} // namespace detail

class TextureBaker {
public:
    TextureMapId add_mesh(TexturedMesh mesh) {
        const TextureMapId id = this->_entries.size();
        this->_entries.emplace_back(std::move(mesh));
        return id;
    }
    TextureMapId add_mesh(std::vector<glm::uvec3> triangles, std::vector<glm::dvec2> uvs, Texture texture) {
        return this->add_mesh(TexturedMesh{
            .triangles = std::move(triangles),
            .map = TextureMap{
                .uvs = std::move(uvs),
                .texture = std::move(texture),
            },
        });
    }

    TextureMapId add_remap(TexturedMesh mesh, std::span<const glm::dvec2> new_uvs) {
        TextureComposition comp;
        comp.maps.push_back(std::move(mesh.map));
        comp.triangles = transform_vector(mesh.triangles, [](const glm::uvec3 &triangle) {
            return MappedTriangle{
                .source_map = 0,
                .source = triangle,
                .target = triangle,
            };
        });
        comp.target_uvs = std::vector<glm::dvec2>(new_uvs.begin(), new_uvs.end());
        return this->add_composition(std::move(comp));
    }

    TextureMapId add_composition(TextureComposition comp) {
        const TextureMapId id = this->_entries.size();
        this->_entries.emplace_back(std::move(comp));
        return id;
    }

    // Places every chart, yielding an atlas that can be rendered.
    PackedAtlas pack() &&;

    // Places every chart and renders at the given size.
    Texture bake(glm::uvec2 texture_size) &&;

private:
    friend class PackedAtlas;

    using Entry = std::variant<TexturedMesh, TextureComposition>;

    atlas::Packing compute_packing() const {
        if (this->_entries.size() == 1) {
            return this->pack_single(this->_entries.front());
        }

        atlas::Packer packer;

        std::vector<glm::uvec3> target_triangles;
        for (const Entry& entry : this->_entries) {
            visit_entry(entry,
                [&](const TexturedMesh& mesh) {
                    packer.add_uv_mesh(mesh.triangles, mesh.map.uvs, get_texture_size(mesh.map.texture));
                },
                [&](const TextureComposition& comp) {
                    target_triangles.clear();
                    target_triangles.reserve(comp.triangles.size());
                    for (const MappedTriangle &triangle : comp.triangles) {
                        target_triangles.push_back(triangle.target);
                    }
                    const uint32_t side = std::ceil(std::sqrt(detail::get_effective_pixel_area(comp)));
                    packer.add_uv_mesh(
                        target_triangles,
                        comp.target_uvs,
                        glm::uvec2(side)
                    );
                });
        }

        return packer.pack();
    }

    atlas::Packing pack_single(const Entry &entry) const {
        return visit_entry<atlas::Packing>(entry,
            [](const TexturedMesh &mesh) {
                return atlas::Packing(mesh.map.uvs);
            },
            [](const TextureComposition &comp) {
                return atlas::Packing(comp.target_uvs);
            });
    }

    static double effective_pixel_area(const Entry &entry) {
        return visit_entry<double>(entry,
            [](const TexturedMesh &mesh) {
                return glm::compMul(glm::dvec2(detail::get_effective_texture_size(mesh.map)));
            },
            [](const TextureComposition &comp) {
                return detail::get_effective_pixel_area(comp);
            });
    }

    template <class R = void, class MeshFn, class CompFn>
    static R visit_entry(
        const Entry &entry,
        MeshFn &&mesh_fn,
        CompFn &&comp_fn) {
        return std::visit(
            [&](const auto &value) -> R {
                using T = std::remove_cvref_t<decltype(value)>;

                if constexpr (std::is_same_v<T, TexturedMesh>) {
                    return std::invoke(mesh_fn, value);
                } else {
                    static_assert(std::is_same_v<T, TextureComposition>);
                    return std::invoke(comp_fn, value);
                }
            },
            entry);
    }

    std::vector<Entry> _entries;
};

// An atlas whose charts are placed but whose pixels are not rendered yet. Owns the baker it
// was packed from, so a packing can never be paired with entries it was not built for.
class PackedAtlas {
public:
    std::span<const glm::dvec2> uvs_for(const TextureMapId id) const {
        return this->_packing.uvs_for_mesh(id);
    }

    float utilization() const {
        return this->_packing.utilization();
    }

    // Triangles the source maps were measured against.
    uint32_t source_triangle_count() const {
        return sum(this->_baker._entries, [](const TextureBaker::Entry &entry) {
            return TextureBaker::visit_entry<uint32_t>(entry,
                [](const TexturedMesh &mesh) {
                    return mesh.triangles.size();
                },
                [](const TextureComposition &comp) {
                    return comp.triangles.size();
                });
        });
    }

    // Texels the source maps hold for the source regions.
    double source_pixel_area() const {
        double area = 0;
        for (const TextureBaker::Entry &entry : this->_baker._entries) {
            area += TextureBaker::effective_pixel_area(entry);
        }
        return area;
    }

    Texture bake(const glm::uvec2 texture_size) const {
        const std::vector<TextureBaker::Entry> &entries = this->_baker._entries;
        if (entries.size() == 1) {
            return TextureBaker::visit_entry<Texture>(entries.front(),
                [&](const TexturedMesh &mesh) {
                    return rescale_texture(mesh.map.texture, texture_size);
                },
                [&](const TextureComposition &) {
                    return this->bake_texture(texture_size);
                });
        }

        return this->bake_texture(texture_size);
    }

private:
    friend class TextureBaker;

    PackedAtlas(TextureBaker baker, atlas::Packing packing)
        : _baker(std::move(baker)), _packing(std::move(packing)) {}

    Texture bake_texture(const glm::uvec2 texture_size) const {
        TextureReprojector reprojector(texture_size, CV_8UC3);

        TextureSet source_images;
        std::vector<ReprojectionTriangle> reprojection_triangles;

        const auto make_triangle = [](
                                       const uint32_t source_image_index,
                                       const glm::uvec3 &source_triangle,
                                       const std::vector<glm::dvec2> &source_uvs,
                                       const glm::uvec3 &target_triangle,
                                       std::span<const glm::dvec2> target_uvs) {
            ReprojectionTriangle triangle;
            triangle.source_image_index = source_image_index;

            triangle.source_uvs = {
                source_uvs[source_triangle.x],
                source_uvs[source_triangle.y],
                source_uvs[source_triangle.z],
            };

            triangle.target_uvs = {
                target_uvs[target_triangle.x],
                target_uvs[target_triangle.y],
                target_uvs[target_triangle.z],
            };

            return triangle;
        };

        for (uint32_t i = 0; i < this->_baker._entries.size(); i++) {
            const auto target_uvs = this->_packing.uvs_for_mesh(i);

            TextureBaker::visit_entry(
                this->_baker._entries[i],
                [&](const TexturedMesh &mesh) {
                    const uint32_t source_image_index = source_images.add(mesh.map.texture);

                    for (const glm::uvec3 &triangle : mesh.triangles) {
                        reprojection_triangles.push_back(
                            make_triangle(
                                source_image_index,
                                triangle,
                                mesh.map.uvs,
                                triangle,
                                target_uvs));
                    }
                },
                [&](const TextureComposition &comp) {
                    std::vector<uint32_t> source_image_indices;
                    source_image_indices.reserve(comp.maps.size());

                    for (const TextureMap &map : comp.maps) {
                        source_image_indices.push_back(source_images.add(map.texture));
                    }

                    for (const MappedTriangle &triangle : comp.triangles) {
                        const TextureMap &source = comp.maps[triangle.source_map];

                        reprojection_triangles.push_back(
                            make_triangle(
                                source_image_indices[triangle.source_map],
                                triangle.source,
                                source.uvs,
                                triangle.target,
                                target_uvs));
                    }
                });
        }

        return reprojector.render(source_images, reprojection_triangles);
    }

    TextureBaker _baker;
    atlas::Packing _packing;
};

inline Texture TextureBaker::bake(const glm::uvec2 texture_size) && {
    return std::move(*this).pack().bake(texture_size);
}

inline PackedAtlas TextureBaker::pack() && {
    atlas::Packing packing = this->compute_packing();
    return PackedAtlas(std::move(*this), std::move(packing));
}
