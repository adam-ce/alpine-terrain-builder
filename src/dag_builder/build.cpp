#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>
#include <glm/gtx/component_wise.hpp>
#include <tbb/concurrent_vector.h>

#include "build.h"
#include "centroids.h"
#include "cluster.h"
#include "clusterize.h"
#include "compact.h"
#include "encoded.h"
#include "error_bounds.h"
#include "int_math.h"
#include "log.h"
#include "merge/clusterings.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/IdRect.h"
#include "octree/OddLevelShifted.h"
#include "octree/Space.h"
#include "octree/Storage.h"
#include "octree/storage/open.h"
#include "octree/traverse.h"
#include "ProgressIndicator.h"
#include "partition.h"
#include "simplify.h"
#include "slice.h"
#include "range_utils.h"
#include "storage.h"
#include "thread_safe_storage.h"
#include "utils.h"
#include "vertex_lock.h"
#include "parallel.h"

namespace dag {

namespace {

// Load a mesh from storage and clusterize it.
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

    LOG_DEBUG(
        "Clustering mesh for node {} with {} vertices and {} triangles",
        id,
        mesh.vertex_count(),
        mesh.face_count());

    return clusterize(mesh);
}

// A clustering and a mapping from its clusters to the original input clusters that were merged to produce it.
struct LodResult {
    Clustering clustering;
    std::vector<std::vector<uint32_t>> child_map;
};

// Run the full LOD pipeline on a clustering: partition, simplify, re-clusterize, and build child map.
LodResult build_lod(const Clustering &input, const BuildOptions &options) {
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

// Load input meshes, clusterize, and filter them to the target region.
std::vector<Clustering> load_input_clusters(
    const std::span<const octree::Id> input_ids,
    const octree::MeshStorage &input_storage,
    const RegionFilter& filter) {
    std::vector<Clustering> result;

    for (const octree::Id &id : input_ids) {
        auto opt = load_and_clusterize_mesh(input_storage, id);
        if (!opt) {
            LOG_WARN("Failed to load or clusterize {}, skipping", id);
            continue;
        }
        Clustering clustering = std::move(*opt);
        if (clustering.is_empty()) {
            LOG_WARN("Loaded clustering was empty for {}, skipping", id);
            continue;
        }
        auto indices = find_clusters_matching(clustering, filter);
        if (indices.empty()) {
            continue;
        }
        result.push_back(slice_clusters(clustering, indices));
    }

    return result;
}

// Dependencies shared across the whole dag building pipeline.
struct BuildContext {
    const octree::IndexedMeshStorage &input_storage;
    ThreadSafeStorage<octree::IndexedDagStorage> output_storage;
    const BuildOptions &options;
    const octree::Space &space;
    const octree::OddLevelShifted &shifted_space;
    const radix::geometry::Aabb3d &root_bounds;
};

// Load relevant DAG nodes, filter, merge, then simplify clusters.
dag::ClusterBatch load_and_simplify_dag_nodes(
    const std::vector<octree::Id> &dag_ids,
    const RegionFilter &filter,
    const double epsilon,
    const BuildContext &ctx) {
    std::vector<dag::Id> cluster_sources;
    std::vector<Clustering> filtered;

    for (const octree::Id &id : dag_ids) {
        const auto dag_node = ctx.output_storage.load(id);
        if (!dag_node) {
            LOG_WARN("Failed to load DAG node {}, skipping", id);
            continue;
        }
        Clustering clustering = std::move(dag_node.value().clustering);
        for (auto &[cluster_index, cluster] : enumerate(clustering.clusters)) {
            cluster.id = cluster_sources.size();
            cluster_sources.emplace_back(id, cluster_index);
        }
        if (clustering.is_empty()) {
            continue;
        }

        auto indices = find_clusters_matching(clustering, filter);
        if (indices.empty()) {
            continue;
        }
        filtered.push_back(slice_clusters(clustering, indices));
    }

    const Clustering merged = merge_clusterings(filtered, epsilon);

    const auto [simplified, child_map] = build_lod(merged, ctx.options);

    auto child_id_map = transform_vector(child_map, [&](const auto &children) {
        return transform_vector(children, [&](const uint32_t merged_index) {
            const uint32_t source_index = merged.clusters[merged_index].id;
            return cluster_sources[source_index];
        });
    });

    return {simplified, std::move(child_id_map)};
}

// Combine input clusters with inner clusters.
dag::ClusterBatch combine_input_and_inner(
    std::vector<Clustering> input_clusters,
    dag::ClusterBatch inner,
    const double epsilon) {
    const bool has_input = !input_clusters.empty();
    const bool has_inner = !inner.clustering.is_empty();
    DEBUG_ASSERT(has_input || has_inner);

    // If there are no input clusters, just return the inner clustering.
    if (!has_input) {
        return inner;
    }

    // If there are no inner clusters, just return the input clusters merged together.
    uint32_t input_cluster_count = 0;
    for (const auto &c : input_clusters) {
        input_cluster_count += c.cluster_count();
    }
    if (!has_inner) {
        Clustering merged = merge_clusterings(std::move(input_clusters), epsilon);
        return dag::ClusterBatch::make_leaves(std::move(merged));
    }

    // Otherwise, merge the input clusters with the inner clustering.
    input_clusters.push_back(std::move(inner.clustering));
    Clustering combined = merge_clusterings(std::move(input_clusters), epsilon);

    std::vector<std::vector<dag::Id>> combined_child_map;
    combined_child_map.reserve(combined.cluster_count());
    for (uint32_t i = 0; i < input_cluster_count; i++) {
        combined_child_map.emplace_back();
    }
    for (auto &entry : inner.child_map) {
        combined_child_map.push_back(std::move(entry));
    }

    return {std::move(combined), std::move(combined_child_map)};
}

// Compute epsilon value for merging clusters based on the size of the node bounds.
double compute_epsilon(const radix::geometry::Aabb3d &bounds) {
    return glm::compAdd(bounds.size()) / 30000.0;
}

// Build a single target node by loading input clusters (preserved as-is) and
// DAG children (inner, run through simplification pipeline), then combining both.
std::optional<dag::ClusterBatch> build_node(
    const octree::Id &target_id,
    const std::vector<octree::Id> &input_ids,
    const std::vector<octree::Id> &dag_ids,
    const BuildContext &ctx) {
    LOG_DEBUG("Building node {} ({} inputs, {} inner nodes)", target_id, input_ids.size(), dag_ids.size());

    const auto node_bounds = ctx.shifted_space.get_node_bounds(target_id);
    const double epsilon = compute_epsilon(node_bounds);

    RegionFilter input_filter;
    input_filter.include = {node_bounds};
    if (ctx.options.include_mode == IncludeMode::CurrentAndCoarser) {
        for (const octree::Id &dag_id : dag_ids) {
            input_filter.exclude.push_back(ctx.shifted_space.get_node_bounds(dag_id));
        }
    }
    std::vector<Clustering> input_clusters = load_input_clusters(input_ids, ctx.input_storage, input_filter);

    RegionFilter dag_filter;
    dag_filter.include = {node_bounds};
    dag::ClusterBatch inner = load_and_simplify_dag_nodes(
        dag_ids, dag_filter, epsilon, ctx);

    if (input_clusters.empty() && inner.clustering.is_empty()) {
        LOG_WARN("No valid clusters for node {}, skipping", target_id);
        return std::nullopt;
    }

    uint32_t input_cluster_count = 0;
    for (const auto &c : input_clusters) {
        input_cluster_count += c.cluster_count();
    }
    LOG_INFO(
        "Finished node {} with {} input + {} inner clusters",
        target_id,
        input_cluster_count,
        inner.clustering.cluster_count());

    return combine_input_and_inner(std::move(input_clusters), std::move(inner), epsilon);
}

struct LevelWorkplan {
    std::vector<octree::Id> targets;
    std::unordered_map<octree::Id, std::vector<octree::Id>> input_sources;
    std::unordered_map<octree::Id, std::vector<octree::Id>> inner_nodes;
};

// Return all input nodes from input_by_level that spatially overlap the given target node.
std::vector<octree::Id> find_relevant_input_nodes(
    const octree::Id &target_id,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const octree::OddLevelShifted &shifted_space,
    const octree::Space &space,
    const IncludeMode include_mode) {
    std::vector<octree::Id> result;
    if (include_mode == IncludeMode::CurrentOnly) {
        // Include same-level inputs that intersect with the target_id in the shifted tree.
        const std::vector<octree::Id> &input_nodes = input_by_level[target_id.level()];
        octree::IdRect standard_nodes = shifted_space.find_intersecting_standard_nodes(target_id);
        result.reserve(standard_nodes.size());
        for (const octree::Id& input_id : input_nodes) {
            if (standard_nodes.contains(input_id)) {
                result.push_back(input_id);
            }
        }
    } else if (include_mode == IncludeMode::CurrentAndCoarser) {
        // Include inputs at same or coarser levels whose bounds intersect target_id in the shifted tree.
        const radix::geometry::Aabb3d target_bounds = shifted_space.get_node_bounds(target_id);
        for (uint32_t level = 0; level <= target_id.level(); level++) {
            const std::vector<octree::Id> &input_nodes = input_by_level[level];
            for (const octree::Id& input_id : input_nodes) {
                const radix::geometry::Aabb3d input_bounds = space.get_node_bounds(input_id);
                if (radix::geometry::intersect(target_bounds, input_bounds)) {
                    result.push_back(input_id);
                }
            }
        }
    }
    return result;
}

// Return the already-built DAG nodes that are children of the given target node.
std::vector<octree::Id> find_relevant_dag_nodes(
    const octree::Id &target_id,
    const std::unordered_set<octree::Id> &prev_level_built,
    const octree::OddLevelShifted &shifted_space) {
    std::vector<octree::Id> result;
    for (const octree::Id &parent : shifted_space.get_intersecting_nodes_on_level(target_id, target_id.level()+1)) {
        if (prev_level_built.contains(parent)) {
            result.push_back(parent);
        }
    }
    return result;
}

// Discover all shifted-space target nodes for one level from inputs and already-built children.
std::unordered_set<octree::Id> find_nodes_to_build_on_level(
    const uint32_t level,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const std::unordered_set<octree::Id> &prev_level_built,
    const BuildContext &ctx) {
    std::unordered_set<octree::Id> target_set;

    // Consider input nodes at this level
    const std::span<const octree::Id> level_input_ids = input_by_level[level];
    for (const octree::Id &input_id : level_input_ids) {
        target_set.insert(input_id);
    }

    // Consider parents of previously built nodes
    for (const octree::Id &child : prev_level_built) {
        const octree::IdRect parents = ctx.shifted_space.get_intersecting_nodes_on_level(child, level);
        for (const octree::Id &parent : parents) {
            target_set.insert(parent);
        }
    }

    return target_set;
}

// Build a workplan for a single level, determining which target nodes to build and which input and DAG nodes are relevant for each target.
LevelWorkplan build_level_workplan(
    const uint32_t level,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const std::unordered_set<octree::Id> &prev_level_built,
    const BuildContext &ctx) {
    std::unordered_set<octree::Id> target_set = find_nodes_to_build_on_level(level, input_by_level, prev_level_built, ctx);

    std::unordered_map<octree::Id, std::vector<octree::Id>> input_sources;
    std::unordered_map<octree::Id, std::vector<octree::Id>> inner_nodes;
    for (const octree::Id &target : target_set) {
        input_sources[target] = find_relevant_input_nodes(target, input_by_level, ctx.shifted_space, ctx.space, ctx.options.include_mode);
        inner_nodes[target] = find_relevant_dag_nodes(target, prev_level_built, ctx.shifted_space);
    }

    return {
        to_vector(target_set),
        std::move(input_sources),
        std::move(inner_nodes)};
}

// Pre-filter input nodes to only those that intersect the target root bounds.
std::vector<std::vector<octree::Id>> gather_relevant_input_leaves(
    const octree::IndexMap &index,
    const octree::Space &space,
    const radix::geometry::Aabb3d &root_bounds)
{
    const auto start = space.find_smallest_node_encompassing_bounds(root_bounds)
        .value_or(octree::Id::root());
    std::vector<std::vector<octree::Id>> result(octree::Id::max_level() + 1);
    octree::traverse(
        index,
        [&](const octree::Id &id, const octree::NodeStatus status) {
            if (status == octree::NodeStatus::Leaf && radix::geometry::intersect(root_bounds, space.get_node_bounds(id))) {
                result[id.level()].push_back(id);
            }
        },
        [&](const octree::Id &id) {
            return radix::geometry::intersect(root_bounds, space.get_node_bounds(id));
        },
        start);
    return result;
}

// Find the maximum level in input_by_level that has any nodes.
std::optional<uint32_t> find_max_input_level(const std::vector<std::vector<octree::Id>> &input_by_level) {
    std::optional<uint32_t> max_level;
    for (uint32_t level = 0; level < input_by_level.size(); level++) {
        if (!input_by_level[level].empty()) {
            max_level = level;
        }
    }
    return max_level;
}

} // namespace

// Build a single level of the DAG, returning the set of nodes that were built.
std::unordered_set<octree::Id> build_level(
    const uint32_t level,
    const std::vector<std::vector<octree::Id>> &input_by_level,
    const std::unordered_set<octree::Id> &prev_level_built,
    const BuildContext &ctx) {
    auto [targets, input_sources, inner_nodes] =
        build_level_workplan(level, input_by_level, prev_level_built, ctx);

    if (targets.empty()) {
        return {};
    }

    LOG_INFO("Building level {} ({} targets)", level, targets.size());

    std::unordered_set<octree::Id> already_built;
    if (ctx.options.resume) {
        for (const octree::Id &target : targets) {
            if (ctx.output_storage.has(target)) {
                already_built.insert(target);
            }
        }
    }

    // Initialize debug storage if requested (contains .glb meshes)
    std::optional<octree::MeshStorage> debug_storage;
    if (ctx.options.write_debug_meshes) {
        debug_storage = octree::open_folder(
            ctx.output_storage.base_path().string() + "-debug",
            false,
            octree::OpenOptions{.preferred_extension_with_dot = ".glb"});
    }

    tbb::concurrent_vector<octree::Id> saved_ids;

    ProgressIndicator progress(targets.size());
    for (size_t i = 0; i < already_built.size(); i++) {
        progress.task_finished();
    }
    auto progress_thread = progress.start_monitoring();

    parallel_foreach(targets, [&](const octree::Id &target) {
        if (already_built.contains(target)) {
            return;
        }

        const auto dag_ids = find_value(inner_nodes, target).value_or({});
        const auto target_input_ids = find_value(input_sources, target).value_or({});

        auto result = build_node(
            target,
            target_input_ids,
            dag_ids,
            ctx);

        if (result) {
            const auto save_result = ctx.output_storage.save(target, *result);
            DEBUG_ASSERT_VAL(save_result);
            if (debug_storage) {
                debug_storage->save(target, clustering_to_mesh(result->clustering));
            }
            saved_ids.push_back(target);
        }
        progress.task_finished();
    }, ctx.options.parallelize);

    progress_thread.join();

    std::unordered_set<octree::Id> built = std::move(already_built);
    for (const octree::Id &id : saved_ids) {
        built.insert(id);
    }

    return built;
}

// Builds the DAG from input_storage into output_storage, restricted to levels within level_range.
// Iterates octree levels from finest to coarsest, simplifying and re-clustering geometry at each
// level from its children.
void build_levels(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options,
    const Range<uint32_t> &level_range) {
    const octree::OddLevelShifted shifted_space = octree::OddLevelShifted::earth();
    const octree::Space space = octree::Space::earth();
    const octree::Id root_node = options.root_node;
    const auto root_bounds = shifted_space.get_node_bounds_with_children(root_node);
    auto input_by_level = gather_relevant_input_leaves(input_storage.index(), space, root_bounds);

    // Find the maximum input level that has any nodes
    auto max_input_level_opt = find_max_input_level(input_by_level);
    if (!max_input_level_opt.has_value()) {
        LOG_WARN("No input nodes found for root {}", root_node);
        return;
    }
    const uint32_t max_input_level = max_input_level_opt.value();

    const Range<uint32_t> valid_range{root_node.level(), max_input_level + 1};
    const Range<uint32_t> range = valid_range.intersection(level_range);
    if (range.empty()) {
        LOG_WARN("Requested level range does not overlap with buildable levels {}-{}", root_node.level(), max_input_level);
        return;
    }

    BuildContext ctx{input_storage, ThreadSafeStorage(std::move(output_storage)), options, space, shifted_space, root_bounds};

    std::unordered_set<octree::Id> prev_level_built;
    for (uint32_t level = range.max; level-- > range.min;) {
        prev_level_built = build_level(
            level,
            input_by_level,
            prev_level_built,
            ctx);
    }

    output_storage = std::move(ctx.output_storage).release();
}

// Builds the complete DAG from input_storage into output_storage.
void build_full(
    const octree::IndexedMeshStorage &input_storage,
    octree::IndexedDagStorage &output_storage,
    const BuildOptions &options) {
    build_levels(input_storage, output_storage, options, full_range<uint32_t>());
}

} // namespace dag
