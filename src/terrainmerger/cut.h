#pragma once

#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "log.h"
#include "mask.h"
#include "merge.h"
#include "octree/Id.h"
#include "octree/NodeStatus.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/storage/open.h"
#include "octree/traverse.h"

inline void cut_dataset(
    const octree::IndexedStorage &input,
    const MeshMask& mask,
    octree::Storage &output) {
    
    mesh::io::save_to_path(mask.mesh, "./mask.glb");

    octree::traverse(
        input.index(),
        [&](const octree::Id &id, const octree::NodeStatus &status) {
            if (status == octree::NodeStatus::Virtual) {
                return;
            }
            DEBUG_ASSERT(status == octree::NodeStatus::Leaf);
            
            const SimpleMesh mesh = DEBUG_ASSERT_VAL(input.read_node(id)).value();
            LOG_TRACE("Cutting mesh at {}", id);
            const ClipResult result = clip_on_mask(mesh, mask);
            if (result.is_unchanged()) {
                LOG_TRACE("Mesh was fully inside the mask");
                DEBUG_ASSERT_VAL(input.copy_node_to(id, output));
            } else {
                const SimpleMesh& clipped_mesh = result.mesh();
                if (clipped_mesh.is_empty()) {
                    LOG_TRACE("Mesh was clipped from {} vertices and {} triangles to {} vertices and {} triangles", 
                        mesh.vertex_count(), mesh.face_count(), clipped_mesh.vertex_count(), clipped_mesh.face_count());
                    DEBUG_ASSERT_VAL(output.write_node(id, result.mesh()));
                } else {
                    LOG_TRACE("Mesh was fully ouside the mask");
                }
            }
        });
    
    output.save_or_create_index();
}

inline void cut_dataset(
    const octree::IndexedStorage &input_dataset,
    const MeshMask& mask,
    const std::filesystem::path &output_path) {
    LOG_TRACE("Creating output dataset at {}", output_path);
    std::filesystem::create_directories(output_path);

    octree::IndexedStorage output_dataset = octree::open_folder_indexed(output_path, octree::disk::layout::strategy::make_default(), ".glb");
    if (!output_dataset.index().empty()) {
        LOG_ERROR_AND_EXIT("Output dataset has to be empty.");
    }

    cut_dataset(input_dataset, mask, output_dataset);
}

