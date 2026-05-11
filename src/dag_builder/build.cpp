#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <ranges>
#include <optional>

#include <fmt/format.h>
#include <tbb/flow_graph.h>
#include <tbb/parallel_for.h>

#include "build.h"
#include "cluster.h"
#include "clusterize.h"
#include "simplify.h"
#include "vertex_lock.h"
#include "encoded.h"
#include "int_math.h"
#include "log.h"
#include "merge.h"
#include "mesh/io.h"
#include "mesh/SimpleMesh.h"
#include "octree/OddLevelShifted.h"
#include "octree/Id.h"
#include "octree/Storage.h"
#include "storage.h"
#include "utils.h"
#include "partition.h"
#include "Range.h"
#include "compact.h"
#include "centroids.h"
#include "error_bounds.h"
#include "slice.h"
#include "ProgressIndicator.h"

namespace dag {

namespace {

std::optional<Clustering> load_and_clusterize_mesh(
    const octree::MeshStorage &storage,
    const octree::Id &id) {
    const auto result = storage.load(id);

    if (!result) {
        if (result.error() != mesh::io::LoadMeshErrorKind::FileNotFound) {
            LOG_ERROR("Failed to read node {}: {}", id, result.error());
        }

        return std::nullopt;
    }

    const mesh::Simple &mesh = result.value();

    // TODO: remove this
    if (!mesh.has_texture() || !mesh.has_uvs()) {
        LOG_WARN("Mesh {} has no texture or UVs, skipping", id);
        return std::nullopt;
    }

    LOG_DEBUG(
        "Clustering mesh for node {} with {} vertices and {} triangles",
        id,
        mesh.vertex_count(),
        mesh.face_count());

    return clusterize(mesh);
}

std::vector<std::unordered_set<octree::Id>> make_nodes_by_level(const octree::IndexMap &index_map) {
    std::vector<std::unordered_set<octree::Id>> nodes_by_level(octree::Id::max_level());
    for (const auto &[id, status] : index_map) {
        if (status == octree::NodeStatus::Leaf) {
            const auto level = id.level();
            nodes_by_level[level].insert(id);
        }
    }
    return nodes_by_level;
}

struct ClusteringAndChildMap {
    Clustering clustering;
    std::vector<std::vector<uint32_t>> child_map;
};

ClusteringAndChildMap build_one(const Clustering &input, const BuildOptions &options) {
    // Partition clusters into groups
    const Partitioning partitioning = create_partitioning(input, PartitionOptions{
                                                                     .clusters_per_partition = options.clusters_per_partition});
    // Construct new clusters and generate new textures.
    Clustering clustering = apply_partitioning(input, partitioning);
    // Create partition to cluster map
    std::vector<std::vector<uint32_t>> partition_to_clusters(partitioning.partition_count);
    for (const auto [cluster_index, partition_index] : enumerate(partitioning.cluster_partitions)) {
        partition_to_clusters[partition_index].push_back(cluster_index);
    }

    // Trim textures
    trim_textures_inplace(clustering);

    // Find vertices to lock
    const std::vector<uint8_t> vertex_lock = find_vertices_to_lock(clustering);

    // Simplify each cluster
    clustering = simplify(clustering, SimplifyOptions{
                                          .target_ratio = options.target_ratio,
                                          .vertex_lock = VertexLock::mask(vertex_lock)});
    remove_unused_vertices_inplace(clustering);

    // Split each cluster into roughly 4 parts
    auto result = clusterize(clustering);
    clustering = result.clustering;

    // Build child map
    std::vector<std::vector<uint32_t>> child_map(clustering.cluster_count());
    for (const auto [final_index, partition_index] : enumerate(result.backward_mapping)) {
        auto &children = child_map[final_index];
        const auto &original_clusters = partition_to_clusters[partition_index];
        for (const uint32_t original_index : original_clusters) {
            children.push_back(original_index);
        }
    }

    return {clustering, child_map};
}

std::vector<octree::Id> find_all_intersecting_nodes_on_levels(
    const octree::Id &id,
    const Range<uint32_t> &level_range,
    const octree::OddLevelShifted &space) {
    std::vector<octree::Id> result;
    for (uint32_t level = level_range.min; level < level_range.max; level++) {
        const auto nodes = space.get_intersecting_nodes_on_level(id, level);
        result.insert(result.end(), nodes.begin(), nodes.end());
    }
    return result;
}

Range<uint32_t> find_level_range_of_nodes(const std::vector<std::unordered_set<octree::Id>> &nodes_by_level) {
    uint32_t min_level = 0;
    for (const auto& nodes : nodes_by_level) {
        if (nodes.empty()) {
            min_level++;
        } else {
            break;
        }
    }

    if (min_level == nodes_by_level.size()) {
        return empty_range<uint32_t>();
    }

    uint32_t max_level = nodes_by_level.size();
    for (const auto& nodes : nodes_by_level | std::views::reverse) {
        if (nodes.empty()) {
            max_level--;
        } else {
            break;
        }
    }

    return {min_level, max_level};
}

void build_range_impl(
    octree::IndexedDagStorage &storage,
    Range<uint32_t> level_range,
    const octree::Id &root_node,
    const BuildOptions &options) {
    const Range<uint32_t> valid_range(0, MAX_LEVEL);
    level_range = level_range.intersection(valid_range);

    std::vector<std::unordered_set<octree::Id>> nodes_by_level = make_nodes_by_level(storage.index());
    const auto leaf_level_range = find_level_range_of_nodes(nodes_by_level);

    LOG_INFO("Building levels {}..{}", level_range.min, level_range.max);

    const octree::OddLevelShifted space = octree::OddLevelShifted::earth();
    for (uint32_t level = level_range.max; level-- > level_range.min;) {
        LOG_INFO("Building level {}", level);

        std::unordered_set<octree::Id> nodes_to_build;
        for (const octree::Id &id : nodes_by_level[level + 1]) {
            // Skip nodes not relevant to selected region
            if (!id.is_descendant_of(root_node, true)) {
                continue;
            }

            const auto parents = space.get_intersecting_nodes_on_level(id, level);
            nodes_to_build.insert(parents.begin(), parents.end());
        }

        LOG_INFO("Building {} nodes for level {}", nodes_to_build.size(), level);
        for (const octree::Id &target_id : nodes_to_build) {
            LOG_TRACE("Processing node {}", target_id);

            // Find relevant nodes to consider
            const auto relevant_levels = leaf_level_range.unite(Range<uint32_t>(level));
            const auto relevant_ids = find_all_intersecting_nodes_on_levels(target_id, relevant_levels, space);
            LOG_TRACE("Node {} has {} relevant source nodes", target_id, relevant_ids.size());

            // Load those nodes actually present
            std::vector<Clustering> source_clusters;
            std::vector<dag::Id> cluster_sources;
            std::vector<octree::Id> source_batch_ids;
            for (const octree::Id &source_id : relevant_ids) {
                if (const auto dag_node = storage.load(source_id)) {
                    Clustering clustering = std::move(dag_node.value().clustering);
                    for (const auto &[cluster_index, cluster] : enumerate(clustering.clusters)) {
                        const uint32_t index_in_sources = cluster_sources.size();
                        cluster_sources.emplace_back(source_id, cluster_index);
                        clustering.clusters[cluster_index].id = index_in_sources;
                    }
                    if (clustering.is_empty()) {
                        continue;
                    }
                    source_clusters.push_back(std::move(clustering));
                    source_batch_ids.push_back(source_id);
                }
            }
            if (source_clusters.empty()) {
                LOG_WARN("No valid relevant nodes found for node {}, skipping", target_id);
                continue;
            }

            // Filter clusterings
            // We keep clusters that
            // - are inside the bounds of the target node
            // - are not contained in the bounds of a smaller source node (to avoid duplicates)
            const auto node_bounds = space.get_node_bounds(target_id);
            std::vector<Clustering> in_bounds_clusters;
            in_bounds_clusters.reserve(source_clusters.size());
            for (const Clustering &clusters : source_clusters) {
                const auto in_bounds_indices = find_clusters_inside_bounds(clusters, node_bounds);
                if (in_bounds_indices.empty()) {
                    continue;
                }

                in_bounds_clusters.push_back(slice_clusters(clusters, in_bounds_indices));
            }

            auto source_level_range = empty_range<uint32_t>();
            for (const octree::Id batch_id : source_batch_ids) {
                source_level_range.expand(batch_id.level());
            }
            std::vector<Clustering> filtered_clusters;
            if (source_level_range.size() == 1) {
                filtered_clusters = std::move(in_bounds_clusters);
            } else {
                std::vector<radix::geometry::Aabb3d> exclusion_bounds;
                for (const octree::Id batch_id : source_batch_ids) {
                    if (batch_id.level() < target_id.level()) {
                        exclusion_bounds.push_back(space.get_node_bounds(batch_id));
                    }
                }

                filtered_clusters.reserve(in_bounds_clusters.size());
                for (const Clustering &clusters : in_bounds_clusters) {
                    const auto filtered_indices = find_clusters_outside_all_bounds(clusters, exclusion_bounds);
                    if (filtered_indices.empty()) {
                        continue;
                    }

                    filtered_clusters.push_back(slice_clusters(clusters, filtered_indices));
                }
            }
            if (filtered_clusters.empty()) {
                continue;
            }

            // Merge clusterings
            LOG_DEBUG("Merging {} clusterings for node {}", filtered_clusters.size(), target_id);
            const double epsilon = glm::compAdd(node_bounds.size()) / 30000;
            const Clustering merged_clusters = merge_clusterings(filtered_clusters, epsilon);

            // Build final clustering
            const auto [final_clustering, child_map] = build_one(merged_clusters, options);

            LOG_INFO(
                "Finished node {} with final clustering of {} vertices and {} clusters",
                target_id,
                final_clustering.vertex_count(),
                final_clustering.cluster_count());

            nodes_by_level[target_id.level()].insert(target_id);

            const auto child_id_map = transform_vector(child_map, [&](const auto &child_ids) {
                return transform_vector(child_ids, [&](const uint32_t merged_index) {
                    const uint32_t source_index = merged_clusters.clusters[merged_index].id;
                    return cluster_sources[source_index];
                });
            });

            const auto result = storage.save(target_id, dag::ClusterBatch(final_clustering, child_id_map));
            DEBUG_ASSERT_VAL(result);
        }
    }
}

} // namespace

template <typename F>
void parallel_for(size_t begin, size_t end, F&& func, bool parallel = true) {
    if (parallel) {
        tbb::parallel_for(begin, end, func);
    } else {
        for (size_t i = begin; i < end; i++) {
            func(i);
        }
    }
}

// Clusterize original dataset and align to shifted octree (by only using even levels)
void build_leaves(
    const octree::IndexedMeshStorage &input_storage,
    octree::DagStorage &output_storage,
    const octree::Id &root_node,
    const bool resume) {

    std::vector<octree::Id> input_nodes;
    input_nodes.reserve(input_storage.index().size());
    for (const auto &[id, status] : input_storage.index()) {
        if (status == octree::NodeStatus::Leaf && root_node.is_ancestor_of(id)) {
            input_nodes.push_back(id);
        }
    }

    ProgressIndicator progress(input_nodes.size());
    auto handle = progress.start_monitoring();

    struct Node {
        octree::Id id;
        dag::ClusterBatch clusters;
    };

    size_t start_index = 0;
    if (resume) {
        for (const auto& [i, id] : enumerate(input_nodes)) {
            if (output_storage.has(id)) {
                start_index = i + 1;
            }
        }
        for (size_t i = 0; i < start_index; i++) {
            progress.task_finished();
        }
    }

    tbb::flow::graph g;
    tbb::flow::function_node<Node> save_node(
        g,
        1, // at most one invocation at a time
        [&](const Node node) {
            if (resume && output_storage.has(node.id)) {
                return;
            }
            const auto result = output_storage.save(node.id, node.clusters);
            DEBUG_ASSERT_VAL(result);
        });

    parallel_for(start_index, input_nodes.size(), [&](const size_t i) {
        const octree::Id id = input_nodes[i];

        const auto result = load_and_clusterize_mesh(input_storage, id);
        if (!result.has_value()) {
            progress.task_finished();
            return;
        }
        const auto clustering = result.value();

        if (is_even(id.level())) {
            save_node.try_put({id, dag::ClusterBatch::make_leaves(clustering)});
        } else {
            const auto assignment = partition_clusters_to_children(clustering, id);
            for (const auto &[child_index, cluster_indices] : enumerate(assignment)) {
                const Clustering child_clustering = slice_clusters(clustering, cluster_indices);
                const octree::Id child_id = id.child(child_index).value();
                if (child_clustering.is_empty()) {
                    progress.task_finished();
                    return;
                }
                save_node.try_put({child_id, dag::ClusterBatch::make_leaves(child_clustering)});
            }
        }
        
        progress.task_finished();
    });

    handle.join();
    g.wait_for_all();
}

void build_full(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options) {
    build_leaves(input_storage, output_storage, octree::Id::root());
    build_full_inner(output_storage, options);
}

void build_full_inner(
    octree::IndexedDagStorage &storage,
    const BuildOptions &options) {
    build_range_impl(storage, full_range<uint32_t>(), octree::Id::root(), options);
}

void build_inner_level(
    octree::IndexedDagStorage &storage,
    const octree::Id &root_node,
    const uint32_t &level,
    const BuildOptions &options) {
    build_range_impl(storage, Range<uint32_t>(level), root_node, options);
}
}
