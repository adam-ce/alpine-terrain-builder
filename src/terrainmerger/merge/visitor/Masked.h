#pragma once

#include <xatlas/xatlas.h>

#include "mask.h"
#include "merge/NodeData.h"
#include "merge/Result.h"
#include "merge/visitor/Visitor.h"
#include "mesh/holes.h"
#include "mesh/combine.h"
#include "mesh/merging/SphereProjectionVertexDeduplicate.h"
#include "mesh/merging/VertexMapping.h"
#include "mesh/merging/helpers.h"
#include "mesh/merging/mapping.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"
#include "spatial_lookup/Hashmap.h"
#include "utils.h"

namespace merge::visitor {

inline xatlas::MeshDecl to_meshdecl(const SimpleMesh &mesh) {
    xatlas::MeshDecl decl{};

    decl.indexCount = static_cast<uint32_t>(mesh.triangles.size() * 3);
    decl.indexData = mesh.triangles.data();
    decl.indexFormat = xatlas::IndexFormat::UInt32;

    decl.vertexCount = static_cast<uint32_t>(mesh.positions.size());
    decl.vertexPositionData = mesh.positions.data();
    decl.vertexPositionStride = sizeof(SimpleMesh::Position);

    if (mesh.has_uvs()) {
        decl.vertexUvData = mesh.uvs.data();
        decl.vertexUvStride = sizeof(SimpleMesh::Uv);
    }

    return decl;
}

inline xatlas::UvMeshDecl to_uvmeshdecl(const SimpleMesh &mesh) {
    xatlas::UvMeshDecl decl{};

    decl.indexCount = static_cast<uint32_t>(mesh.triangles.size() * 3);
    decl.indexData = mesh.triangles.data();
    decl.indexFormat = xatlas::IndexFormat::UInt32;

    ASSERT(mesh.has_uvs());
    decl.vertexUvData = mesh.uvs.data();
    decl.vertexCount = static_cast<uint32_t>(mesh.uvs.size());
    decl.vertexStride = sizeof(SimpleMesh::Uv);

    return decl;
}

struct Atlas {
    std::vector<std::vector<SimpleMesh::Uv>> uvs;
    SimpleMesh::Texture texture;
};

inline uint32_t next_pow2(uint32_t v) {
    if (v <= 1) {
        return 1;
    }
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}

inline uint32_t compute_resolution(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes) {
    size_t total = 0;
    for (const SimpleMesh &m : meshes) {
        total += m.vertex_count();
    }
    const uint32_t result = next_pow2(static_cast<uint32_t>(std::ceil(std::sqrt(total))));
    return std::clamp(result, 512u, 4096u);
}

inline Atlas generate_texture_atlas(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes
) {
    xatlas::Atlas *atlas = xatlas::Create();

    for (const SimpleMesh &mesh : meshes) {
        const xatlas::UvMeshDecl decl = to_uvmeshdecl(mesh);
        const xatlas::AddMeshError error = xatlas::AddUvMesh(atlas, decl);
        if (error != xatlas::AddMeshError::Success) {
            xatlas::Destroy(atlas);
            LOG_ERROR_AND_EXIT("Error adding mesh {}", xatlas::StringForEnum(error));
        }
    }

    xatlas::ChartOptions chartOptions{};
    chartOptions.useInputMeshUvs = true;

    xatlas::PackOptions packOptions{};
    packOptions.padding = 2;
    packOptions.resolution = compute_resolution(meshes);

    xatlas::Generate(atlas, chartOptions, packOptions);

    Atlas result;
    result.uvs.resize(meshes.size());
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh & mesh = meshes[mesh_index];
        result.uvs[mesh_index].resize(mesh.vertex_count());
    }

    for (size_t mesh_index = 0; mesh_index < atlas->meshCount; mesh_index++) {
        const xatlas::Mesh &xmesh = atlas->meshes[mesh_index];

        for (size_t vertex_index = 0; vertex_index < xmesh.vertexCount; vertex_index++) {
            const xatlas::Vertex& vertex = xmesh.vertexArray[vertex_index];
            result.uvs[mesh_index][vertex.xref] = glm::dvec2(vertex.uv[0], vertex.uv[1]);
        }
    }


    cv::Mat mat(atlas->height, atlas->width, CV_8UC4, atlas->image);
    result.texture = mat.clone();

    xatlas::Destroy(atlas);

    return result;
}

// Simple grid-based atlas using the largest texture size
inline Atlas generate_texture_atlas_grid(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes) {
    // Identify meshes that actually have textures
    std::vector<size_t> meshes_with_textures;
    int max_texture_width = 0;
    int max_texture_height = 0;

    for (size_t i = 0; i < meshes.size(); ++i) {
        if (meshes[i].get().texture.has_value()) {
            meshes_with_textures.push_back(i);
            const cv::Mat &tex = meshes[i].get().texture.value();
            max_texture_width = std::max(max_texture_width, tex.cols);
            max_texture_height = std::max(max_texture_height, tex.rows);
        }
    }

    if (meshes_with_textures.empty()) {
        return Atlas{}; // No textures to pack
    }

    size_t number_of_textured_meshes = meshes_with_textures.size();
    size_t grid_size = static_cast<size_t>(std::ceil(std::sqrt(number_of_textured_meshes)));

    int atlas_width = next_pow2(static_cast<uint32_t>(grid_size * max_texture_width));
    int atlas_height = next_pow2(static_cast<uint32_t>(grid_size * max_texture_height));

    // Create atlas image with transparent background
    cv::Mat atlas_image(atlas_height, atlas_width, meshes[meshes_with_textures[0]].get().texture->type(), cv::Scalar(0, 0, 0, 0));

    Atlas result;
    result.uvs.resize(meshes.size());

    size_t slot_index = 0;
    for (size_t mesh_index : meshes_with_textures) {
        const SimpleMesh &mesh = meshes[mesh_index].get();
        int row = slot_index / grid_size;
        int column = slot_index % grid_size;

        // Compute destination rectangle in atlas
        cv::Rect destination_rectangle(
            column * max_texture_width,
            row * max_texture_height,
            mesh.texture->cols,
            mesh.texture->rows);

        // Copy the mesh's texture into the atlas
        mesh.texture->copyTo(atlas_image(destination_rectangle));

        // Update UV coordinates to map the mesh's full texture to its slot
        result.uvs[mesh_index].resize(mesh.vertex_count());
        double u_start = double(column * max_texture_width) / atlas_width;
        double v_start = double(row * max_texture_height) / atlas_height;
        double u_end = double(column * max_texture_width + mesh.texture->cols) / atlas_width;
        double v_end = double(row * max_texture_height + mesh.texture->rows) / atlas_height;

        for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); ++vertex_index) {
            const auto &original_uv = mesh.uvs[vertex_index];
            result.uvs[mesh_index][vertex_index] = glm::dvec2(
                u_start + original_uv.x * (u_end - u_start),
                v_start + original_uv.y * (v_end - v_start));
        }

        ++slot_index;
    }

    result.texture = atlas_image;
    return result;
}

class Masked {
public:
    using Status = octree::NodeStatusOrMissing;
    struct Context {
        MeshMask mask;
        bool has_left_parent;
        bool has_right_parent;
    };
    using Result = merge::Result<Context>;

    explicit Masked(MeshMask mask, octree::Space space) : _mask(mask), _space(space) { }

    Context make_root_context() {
        return Context{
            .mask = this->_mask,
            .has_left_parent = false,
            .has_right_parent = false,
        };
    }

    template <Status LeftStatus, Status RightStatus>
    Result visit(
        const octree::Id &id,
        const NodeData<LeftStatus> &left,
        const NodeData<RightStatus> &right,
        const Context &ctx) {
        if constexpr (left.status() == Status::Inner || right.status() == Status::Inner) {
            UNREACHABLE();
        }

        if constexpr (left.status() == Status::Missing && right.status() == Status::Missing) {
            DEBUG_ASSERT(!(ctx.has_left_parent && ctx.has_right_parent));
            if (!ctx.has_left_parent && !ctx.has_right_parent) {
                return Ignore{};
            }
        }

        // If the parent mask is empty, we can just return whatevers on the left
        if constexpr (left.status() != Status::Missing) {
            if (ctx.mask.mesh.is_empty()) {
                return Unchanged{Source::Left};
            }
        }
        
        LOG_TRACE("Clipping mask for {}", id);
        const auto bounds = pad_bounds(this->_space.get_node_bounds(id), 1.1);
        MeshMask mask(mesh::clip_on_bounds_and_cap(ctx.mask.mesh, bounds));

        // Same when the current mask is empty
        if constexpr (left.status() != Status::Missing) {
            if (mask.mesh.is_empty()) {
                return Unchanged{Source::Left};
            }
        }

        // If we have two leaf nodes we can directly merge them.
        if constexpr (left.status() == Status::Leaf && right.status() == Status::Leaf) {
            return this->merge_meshes(left.mesh(), right.mesh(), true, true, mask);
        }

        // If we have a left leaf we either directly return it (after clipping)
        // or merge if right has a non-missing parent
        if constexpr (left.status() == Status::Leaf && right.status() == Status::Missing) {
            if (ctx.has_right_parent) {
                return this->merge_meshes(left.mesh(), right.mesh().value(), true, false, mask);
            }

            // Right node is actually missing, just return the clipped left node
            auto result = clip_on_mask(left.mesh(), mask, false);
            if (result->is_empty()) {
                return Ignore{};
            } else if (result.is_ref()) {
                return Unchanged{Source::Left};
            } else {
                return Merged{result};
            }
        }
        
        // If we have a right leaf we either directly return it (after clipping)
        // or merge if left has a non-missing parent
        if constexpr (left.status() == Status::Missing && right.status() == Status::Leaf) {
            if (ctx.has_left_parent) {
                return this->merge_meshes(left.mesh().value(), right.mesh(), false, true, mask);
            }

            // Left node is actually missing, just return the clipped right node
            auto result = clip_on_mask(right.mesh(), mask, true);
            if (result->is_empty()) {
                return Ignore{};
            } else if (result.is_ref()) {
                return Unchanged{Source::Right};
            } else {
                return Merged{result};
            }
        }

        if constexpr (left.status() == Status::Missing && right.status() == Status::Missing) {
            DEBUG_ASSERT(ctx.has_left_parent || ctx.has_right_parent);
            if (ctx.has_left_parent) {
                auto result = clip_on_mask(left.mesh().value(), mask, false);
                if (result->is_empty()) {
                    return Ignore{};
                } else {
                    return Merged{result};
                }
            }
            if (ctx.has_right_parent) {
                auto result = clip_on_mask(right.mesh().value(), mask, true);
                if (result->is_empty()) {
                    return Ignore{};
                } else {
                    return Merged{result};
                }
            }
        }

        if constexpr (left.status() == Status::Virtual || right.status() == Status::Virtual) {
            return Recurse{
                Context{
                    .mask = mask,
                    .has_left_parent = ctx.has_left_parent || left.status() == Status::Leaf,
                    .has_right_parent = ctx.has_right_parent || right.status() == Status::Leaf
                }
            };
        }

        UNREACHABLE();
    }

private:
    Result merge_meshes(
        const SimpleMesh &base_mesh,
        const SimpleMesh &new_mesh,
        const bool can_ref_base_mesh,
        const bool can_ref_new_mesh,
        const MeshMask &mask) {
        const Cow<const SimpleMesh> new_mesh_clipped = clip_on_mask(new_mesh, mask, true);
        const Cow<const SimpleMesh> base_mesh_clipped = clip_on_mask(base_mesh, mask, false);

        if (base_mesh_clipped.is_ref() && new_mesh_clipped->is_empty()) {
            if (can_ref_base_mesh) {
                return Unchanged{Source::Left};
            } else {
                return Merged{base_mesh_clipped};
            }
        }
        if (new_mesh_clipped.is_ref() && base_mesh_clipped->is_empty()) {
            if (can_ref_new_mesh) {
                return Unchanged{Source::Right};
            } else {
                return Merged{new_mesh_clipped};
            }
        }

        // Merging the two meshes
        LOG_TRACE("Calculating merge mapping");
        const std::array<std::reference_wrapper<const SimpleMesh>, 2> meshes = {base_mesh_clipped.get(), new_mesh_clipped.get()};
        const double distance_epsilon = mesh::merging::estimate_merge_epsilon(meshes);
        // We have to use a hashmap here instead of a grid since we dont know
        // the bounds of the mesh projected onto the sphere with the deduplication radius.
        spatial_lookup::Hashmap3d<mesh::merging::VertexId> map(distance_epsilon * 3);
        // Deduplication happens by projecting all points onto a sphere and performing epsilon checks there
        const glm::dvec3 tangent_point = meshes[0].get().positions[0];
        const double radius = glm::length(tangent_point);
        mesh::merging::SphereProjectionVertexDeduplicate<
            mesh::merging::VertexId,
            decltype(map)
        > deduplicate(map, distance_epsilon, radius);
        const mesh::merging::VertexMapping mapping = mesh::merging::create_mapping(meshes,
            mesh::merging::create_options().deduplicate(deduplicate).only_consider_boundary(true));

        LOG_TRACE("Creating combined mesh");
        std::vector<size_t> vertex_offsets;
        SimpleMesh merged_mesh = mesh::combine(meshes, vertex_offsets);

        // Find holes between meshes
        LOG_TRACE("Triangulating region between meshes");
        const std::vector<std::vector<mesh::merging::VertexId>> holes = mesh::find_holes_between_meshes(meshes, mapping);
        std::vector<std::vector<uint32_t>> mapped_holes;
        mapped_holes.reserve(holes.size());
        for (const auto& hole : holes) {
            std::vector<uint32_t> mapped_hole;
            mapped_hole.reserve(hole.size());
            for (const auto& vertex_id : hole) {
                const size_t mapped_index = vertex_offsets[vertex_id.mesh_index] + vertex_id.vertex_index;
                mapped_hole.push_back(mapped_index);
            }
        }

        LOG_TRACE("Generating combined texture");
        Atlas atlas = generate_texture_atlas_grid(meshes);
        merged_mesh.texture = std::move(atlas.texture);
        for (size_t mesh_index=0; mesh_index<meshes.size(); mesh_index++) {
            const SimpleMesh &mesh = meshes[mesh_index];
            for (size_t vertex_index=0; vertex_index<mesh.vertex_count(); vertex_index++) {
                merged_mesh.uvs[vertex_offsets[mesh_index] + vertex_index] = atlas.uvs[mesh_index][vertex_index];
            }
        }

        mesh::fill_planar_holes(merged_mesh, mapped_holes);

        // SimpleMesh merged_mesh = mesh::merging::apply_mapping(meshes, mapping, 
        //    mesh::merging::apply_options().deduplicate_triangles(false).merge_uvs(false));
        
        // Generate geometry between the two meshes to fill holes on the merge border
        // LOG_TRACE("Filling holes on merge border");
        // mesh::fill_holes_on_merge_border(merged_mesh, mapping);




        return Merged{merged_mesh};

        /*
        TODO: Advanced merging
        const auto base_mesh_bounds = calculate_bounds(base_mesh);
        auto bounds = base_mesh_bounds;
        bounds.expand_by(new_mesh_bounds);
        const glm::dvec3 tangent_point = bounds.centre();
        const glm::dvec2 radius_range = mask::pad_radius_range(mask::calculate_radius_range(bounds), 2);
        if (mask.has_value()) {
            auto result = mesh::intersection_and_difference(new_mesh, mask->mesh);
            const SimpleMesh new_mesh_in_mask = std::move(result.intersection);
            const SimpleMesh new_mesh_out_mask = std::move(result.difference);
            const MeshMask mask_base_mesh = mask::create_from_mesh(base_mesh, tangent_point, radius_range);
            const SimpleMesh new_mesh_out_mask_clipped = mesh::clip_on_mesh(new_mesh_out_mask, mask_base_mesh.mesh);
            const MeshMask mask_new_mesh_in_mask = mask::create_from_mesh(new_mesh_in_mask, tangent_point, radius_range);
            const SimpleMesh base_mesh_clipped = mesh::clip_on_mesh(base_mesh, mask_new_mesh_in_mask.mesh);
            return mesh::merge::merge_meshes(new_mesh_out_mask_clipped, base_mesh_clipped, new_mesh_in_mask);
        } else {
            const MeshMask mask_new_mesh = mask::create_from_mesh(new_mesh, tangent_point, radius_range);
            const SimpleMesh base_mesh_clipped = mesh::clip_on_mesh(base_mesh, mask_new_mesh.mesh);
            return mesh::merge::merge_meshes(base_mesh_clipped, new_mesh);
        }
        */
    }

    MeshMask _mask = {};
    octree::Space _space;
};

static_assert(Visitor<Masked>);

} // namespace merge::visitor
