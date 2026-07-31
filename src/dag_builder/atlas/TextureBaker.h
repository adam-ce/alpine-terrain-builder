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
#include "number_utils.h"
#include "opencv_utils.h"
#include "range_utils.h"
#include "TextureSet.h"

using Texture = cv::Mat;
using TextureMapId = uint32_t;

class TextureCake {
public:
    const cv::Mat &texture() const {
        return this->_texture;
    }

    std::span<const glm::dvec2> uvs_for(TextureMapId id) const {
        return this->_packing.uvs_for_mesh(id);
    }

    const std::vector<glm::dvec2> &uvs() const {
        return this->_packing.uvs();
    }

    std::vector<glm::dvec2>& uvs() {
        return this->_packing.uvs();
    }

private:
    friend class TextureBaker;

    Texture _texture;
    atlas::Packing _packing;
};

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
inline glm::uvec2 get_texture_size(const Texture &texture) {
    return {texture.cols, texture.rows};
}

inline glm::uvec2 get_effective_texture_size(const TextureMap &map) {
    const radix::geometry::Aabb2d uv_bounds = calculate_bounds(map.uvs);
    const glm::dvec2 size = get_texture_size(map.texture);
    return glm::uvec2(glm::ceil(size * uv_bounds.size()));
}

inline glm::uvec2 get_effective_texture_size(const TextureComposition &comp) {
    double area = 0;
    for (const auto &map : comp.maps) {
        area += glm::compMul(glm::dvec2(get_effective_texture_size(map)));
    }
    const double side = std::max(std::ceil(std::sqrt(area)), 1.0);
    return glm::uvec2(side);
}
} // namespace detail

// Resolution needed to give a compression_ratio fraction of effective_pixel_area
// worth of resolution, after accounting for atlas packing waste (unused texels
// between packed charts), capped at max_texture_size.
inline glm::uvec2 compute_bake_texture_size(const double effective_pixel_area, const float utilization, const double compression_ratio, const uint32_t max_texture_size) {
    const double target_area = effective_pixel_area * compression_ratio / std::max<double>(utilization, 1e-3);
    const uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(target_area)));
    return glm::uvec2(std::min(next_power_of_two(side), max_texture_size));
}

inline glm::uvec2 compute_bake_texture_size(const atlas::Packing &packing, const double compression_ratio, const uint32_t max_texture_size) {
    return compute_bake_texture_size(packing.effective_pixel_area(), packing.utilization(), compression_ratio, max_texture_size);
}

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

    // Exact output resolution.
    TextureCake bake(const glm::uvec2 texture_size) const {
        return this->bake(this->pack(), texture_size);
    }

    // Runs atlas packing (chart computation for multi-mesh atlases; a cheap
    // passthrough for a single entry). Callers that need packing.utilization()
    // or packing.effective_pixel_area() to choose an output resolution should
    // call this once and pass the result to bake(packing, texture_size) below,
    // rather than letting bake(texture_size) compute it again.
    atlas::Packing pack() const {
        if (this->_entries.size() == 1) {
            return this->pack_single(this->_entries.front());
        }

        atlas::Packer packer;

        std::vector<glm::uvec3> target_triangles;
        for (const Entry& entry : this->_entries) {
            visit_entry(entry,
                [&](const TexturedMesh& mesh) {
                    packer.add_uv_mesh(mesh.triangles, mesh.map.uvs, detail::get_effective_texture_size(mesh.map));
                },
                [&](const TextureComposition& comp) {
                    target_triangles.clear();
                    target_triangles.reserve(comp.triangles.size());
                    for (const MappedTriangle &triangle : comp.triangles) {
                        target_triangles.push_back(triangle.target);
                    }
                    packer.add_uv_mesh(
                        target_triangles,
                        comp.target_uvs,
                        detail::get_effective_texture_size(comp)
                    );
                });
        }

        return packer.pack();
    }

    // Bakes using an already-computed packing (e.g. from pack()), so no atlas repacking happens.
    TextureCake bake(const atlas::Packing &packing, const glm::uvec2 texture_size) const {
        if (this->_entries.size() == 1) {
            return visit_entry<TextureCake>(this->_entries.front(),
                [&](const TexturedMesh &mesh) {
                    TextureCake cake;
                    cake._packing = packing;
                    cake._texture = rescale_texture(mesh.map.texture, texture_size);
                    return cake;
                },
                [&](const TextureComposition &) {
                    TextureCake cake;
                    cake._packing = packing;
                    cake._texture = this->bake_texture(cake._packing, texture_size);
                    return cake;
                });
        }

        TextureCake cake;
        cake._packing = packing;
        cake._texture = this->bake_texture(cake._packing, texture_size);
        return cake;
    }

private:
    using Entry = std::variant<TexturedMesh, TextureComposition>;

    Texture bake_texture(const atlas::Packing &packing, const glm::uvec2 texture_size) const {
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

        for (uint32_t i = 0; i < this->_entries.size(); i++) {
            const auto target_uvs = packing.uvs_for_mesh(i);

            visit_entry(
                this->_entries[i],
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

    atlas::Packing pack_single(const Entry &entry) const {
        const double area = effective_pixel_area(entry);
        return visit_entry<atlas::Packing>(entry,
            [&](const TexturedMesh &mesh) {
                return atlas::Packing(mesh.map.uvs, area);
            },
            [&](const TextureComposition &comp) {
                return atlas::Packing(comp.target_uvs, area);
            });
    }

    static double effective_pixel_area(const Entry &entry) {
        return visit_entry<double>(entry,
            [](const TexturedMesh &mesh) {
                return glm::compMul(glm::dvec2(detail::get_effective_texture_size(mesh.map)));
            },
            [](const TextureComposition &comp) {
                return glm::compMul(glm::dvec2(detail::get_effective_texture_size(comp)));
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
