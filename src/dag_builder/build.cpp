#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>
#include <glm/gtx/component_wise.hpp>
#include <tbb/concurrent_vector.h>

#include "build.h"
#include "build_config.h"
#include "centroids.h"
#include "cluster.h"
#include "clusterize.h"
#include "compact.h"
#include "geometry/geometry.h"
#include "numeric/int_math.h"
#include "log.h"
#include "merge/clusterings.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/IdRect.h"
#include "octree/OddLevelShifted.h"
#include "octree/Space.h"
#include "octree/storage/open.h"
#include "store/traverse.h"
#include "ProgressIndicator.h"
#include "partition.h"
#include "simplify.h"
#include "slice.h"
#include "range_utils.h"
#include "storage.h"
#include "thread_safe_storage.h"
#include "store/describe_error.h"
#include "utils.h"
#include "vertex_lock.h"
#include "parallel.h"
#include "ContinuationMode.h"
#include "sf/validate_index.h"

namespace dag {

namespace {

// Load a mesh from storage and clusterize it.
std::optional<Clustering> load_and_clusterize_mesh(
    const mesh::storage::Storage &storage,
    const octree::Id &id) {
    const auto result = storage.load(id);

    if (!result) {
        const auto *codec_error = std::get_if<store::CodecError>(&result.error());
        if (codec_error == nullptr
            || codec_error->category != store::CodecErrorCategory::FileNotFound) {
            LOG_ERROR(
                "Failed to read node {}: {}",
                id,
                store::describe_error(result.error()));
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

// A clustering with its group structure.
struct LodResult {
    Clustering clustering;
    std::vector<uint32_t> group_assignment; // per cluster group index
    PartitionToClusters group_children; // per group child indices
};

// Run the full LOD pipeline on a clustering: partition, simplify, re-clusterize, and build group structure.
LodResult build_lod(
    const Clustering &clusters,
    const BuildOptions &options,
    const radix::geometry::Aabb3d &node_bounds,
    const RegionFilter &lock_filter) {
    const Partitioning partitioning = create_partitioning(clusters, PartitionOptions{
                                                                        .clusters_per_partition = options.clusters_per_partition});
    // Apply the partitioning on geometry only.
    Clustering clustering = merge_clusters(clusters, partitioning);
    PartitionToClusters partition_to_clusters = invert_partitioning(partitioning);

    // Find vertices to lock
    const std::vector<uint8_t> vertex_lock = find_vertices_to_lock(clustering, lock_filter);

    // Convert relative target error (a fraction of the node bounds) to absolute
    const std::optional<float> absolute_target_error = map(options.relative_target_error, [&](const float relative_error) {
        return relative_error * glm::compMax(node_bounds.size());
    });
    const SimplifyOptions simplify_options{
        .target_ratio = options.target_ratio,
        .absolute_target_error = absolute_target_error,
        .vertex_lock = VertexLock::mask(vertex_lock),
        .error_mode = ErrorMode::Add,
        .preserve_cluster_count = true
    };
    clustering = simplify(clustering, simplify_options);
    remove_duplicate_triangles_inplace(clustering);

    // Unwrap the surviving geometry and render its texture.
    clustering = texture_clusters(std::move(clustering), clusters, partition_to_clusters, options.texture_options);

    // Compact the vertex buffer
    remove_unused_vertices_inplace(clustering);

    // Split each cluster into parts again.
    auto result = clusterize(clustering);

    return {std::move(result.clustering), std::move(result.backward_mapping), std::move(partition_to_clusters)};
}

// Load input meshes, clusterize, and filter them to the target region.
std::vector<Clustering> load_input_clusters(
    const std::span<const octree::Id> input_ids,
    const mesh::storage::Storage &input_storage,
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
    const mesh::storage::IndexedStorage &input_storage;
    ThreadSafeStorage<dag::storage::IndexedStorage> output_storage;
    const BuildOptions &options;
    const octree::Space &space;
    const octree::OddLevelShifted &shifted_space;
    const radix::geometry::Aabb3d &root_bounds;
};

// Assemble a node's metadata from a build_lod result.
dag::NodeMetadata build_node_metadata(
    std::vector<uint32_t> group_assignment,
    const PartitionToClusters &group_children,
    const Clustering &merged,
    const Clustering &simplified,
    const std::vector<dag::Id> &cluster_sources,
    const std::unordered_map<octree::Id, dag::NodeMetadata> &cluster_metadata) {
    dag::NodeMetadata metadata;
    metadata.group_assignment = std::move(group_assignment);
    metadata.groups.resize(group_children.segment_count());

    // map child indices to dag ids
    for (const auto [group_index, local_indices] : enumerate(group_children.segments())) {
        metadata.groups[group_index].children = transform_vector(local_indices, [&](const uint32_t merged_index) {
            return cluster_sources[merged.clusters[merged_index].id];
        });
    }

    // compute group bounds
    for (dag::Group &group : metadata.groups) {
        for (const dag::Id &child : group.children) {
            group.bounds.expand_by(dag::get_group_bounds(cluster_metadata.at(child.source_batch), child.cluster_index));
        }
    }

    // compute group error
    for (const auto &[final_index, group_index] : enumerate(metadata.group_assignment)) {
        double &error = metadata.groups[group_index].error;
        error = std::max(error, simplified.clusters[final_index].absolute_error);
    }

    return metadata;
}

// Load relevant DAG nodes, filter, merge, then simplify clusters.
dag::ClusterBatch load_and_simplify_dag_nodes(
    const std::vector<octree::Id> &dag_ids,
    const RegionFilter &filter,
    const double epsilon,
    const radix::geometry::Aabb3d &node_bounds,
    const BuildContext &ctx) {
    std::vector<dag::Id> cluster_sources;
    std::vector<Clustering> filtered;
    std::unordered_map<octree::Id, dag::NodeMetadata> cluster_metadata;

    for (const octree::Id &id : dag_ids) {
        // Load dag node clusters
        auto dag_node = ctx.output_storage.load(id);
        if (!dag_node) {
            LOG_WARN("Failed to load DAG node {}, skipping", id);
            continue;
        }
        auto &[metadata, clustering] = dag_node.value();
        cluster_metadata.emplace(id, std::move(metadata));

        // Assign canonical cluster ids
        for (auto &[cluster_index, cluster] : enumerate(clustering.clusters)) {
            cluster.id = cluster_sources.size();
            cluster_sources.emplace_back(id, cluster_index);
        }
        if (clustering.is_empty()) {
            continue;
        }

        // Find relevant clusters from this node.
        auto indices = find_clusters_matching(clustering, filter);
        if (indices.empty()) {
            continue;
        }
        filtered.push_back(slice_clusters(clustering, indices));
    }
    if (filtered.empty()) {
        return {};
    }

    // Merge remaining clusterings
    const Clustering merged = merge_clusterings(filtered, epsilon);

    // Run nanite-style grouping, simplification, splitting.
    auto [simplified, group_assignment, group_children] = build_lod(merged, ctx.options, node_bounds, filter);

    dag::NodeMetadata metadata = build_node_metadata(std::move(group_assignment), group_children, merged, simplified, cluster_sources, cluster_metadata);

    return {std::move(metadata), std::move(simplified)};
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
    if (!has_inner) {
        Clustering merged = merge_clusterings(std::move(input_clusters), epsilon);
        return dag::make_leaf_batch(std::move(merged));
    }

    // Otherwise, merge the input clusters with the inner clustering.
    std::vector<dag::NodeMetadata> parts = transform_vector(input_clusters, [](const Clustering &input) {
        return dag::build_leaf_metadata(input);
    });
    parts.push_back(std::move(inner.metadata));
    dag::NodeMetadata metadata = concat_metadata(std::move(parts));

    input_clusters.push_back(std::move(inner.clustering));
    Clustering combined = merge_clusterings(std::move(input_clusters), epsilon);

    return {std::move(metadata), std::move(combined)};
}

// Compute epsilon value for merging clusters based on the size of the node bounds.
double compute_epsilon(const radix::geometry::Aabb3d &) {
    // only needed for .glb input
    // return glm::compAdd(bounds.size()) / 1'000'000;
    return 0.0;
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

    // Prepare filter to only include clusters inside the target_id bounds. 
    // In CurrentAndCoarser mode, input regions represented by the relevant DAG nodes are excluded as well.
    RegionFilter input_filter;
    input_filter.include = {node_bounds};
    if (ctx.options.include_mode == IncludeMode::CurrentAndCoarser) {
        for (const octree::Id &dag_id : dag_ids) {
            input_filter.exclude.push_back(ctx.shifted_space.get_node_bounds(dag_id));
        }
    }
    std::vector<Clustering> input_clusters = load_input_clusters(input_ids, ctx.input_storage, input_filter);

    // Mirror of input_filter: exclude the input regions so the seam vertices stay locked.
    RegionFilter dag_filter;
    dag_filter.include = {node_bounds};
    if (ctx.options.include_mode == IncludeMode::CurrentAndCoarser) {
        for (const octree::Id &input_id : input_ids) {
            dag_filter.exclude.push_back(ctx.space.get_node_bounds(input_id));
        }
    }
    dag::ClusterBatch inner = load_and_simplify_dag_nodes(dag_ids, dag_filter, epsilon, node_bounds, ctx);

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

// Nodes one level finer that may hold data belonging to the given node.
octree::IdRect find_relevant_dag_nodes(
    const octree::Id &target_id,
    const octree::OddLevelShifted &shifted_space) {
    // We cannot rely on the intersecting nodes on the next level, since some neighbours clusters may has a centroid in this node's bounds. This can happen when the groups is split at the end of the pipeline.
    const radix::geometry::Aabb3d padded_bounds = geometry::pad_bounds_relative(shifted_space.get_node_bounds(target_id), 0.5 - 1e-6);
    const radix::geometry::Aabb3d search_bounds = radix::geometry::intersection(padded_bounds, shifted_space.bounds());
    return shifted_space.get_intersecting_nodes_on_level(search_bounds, target_id.level() + 1);
}

// Nodes one level finer that may hold data belonging to the given node and are resident.
std::vector<octree::Id> find_relevant_resident_dag_nodes(
    const octree::Id &target_id,
    const std::unordered_set<octree::Id> &prev_level_built,
    const octree::OddLevelShifted &shifted_space) {
    std::vector<octree::Id> result;
    for (const octree::Id &child : find_relevant_dag_nodes(target_id, shifted_space)) {
        if (prev_level_built.contains(child)) {
            result.push_back(child);
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
        const octree::IdRect shifted_nodes = ctx.shifted_space.find_intersecting_nodes_for_standard_id(input_id);
        for (const octree::Id &shifted_id : shifted_nodes) {
            target_set.insert(shifted_id);
        }
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
        inner_nodes[target] = find_relevant_resident_dag_nodes(target, prev_level_built, ctx.shifted_space);
    }

    return {
        to_vector(target_set),
        std::move(input_sources),
        std::move(inner_nodes)};
}

// Pre-filter input nodes to only those that intersect the target root bounds.
std::expected<std::vector<std::vector<octree::Id>>, sf::InvalidTopology> gather_relevant_input_leaves(
    const store::Index<octree::StoreTraits> &index,
    const octree::Space &space,
    const radix::geometry::Aabb3d &root_bounds)
{
    const auto start = space.find_smallest_node_encompassing_bounds(root_bounds)
        .value_or(octree::Id::root());
    std::vector<std::vector<octree::Id>> result(octree::Id::max_level() + 1);
    const auto traversal = store::traverse(
        index,
        [&](const octree::Id &id, const store::NodeStatus status) {
            if (status == store::NodeStatus::Leaf && radix::geometry::intersect(root_bounds, space.get_node_bounds(id))) {
                result[id.level()].push_back(id);
            }
        },
        [&](const octree::Id &id) {
            return radix::geometry::intersect(root_bounds, space.get_node_bounds(id));
        },
        start);
    if (!traversal.has_value()) {
        return std::unexpected(sf::InvalidTopology{traversal.error().key});
    }
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
    if (ctx.options.continuation_mode != ContinuationMode::Overwrite) {
        for (const octree::Id &target : targets) {
            if (DEBUG_ASSERT_VAL(ctx.output_storage.has(target)).value()) {
                already_built.insert(target);
            }
        }
    }

    if (ctx.options.continuation_mode == ContinuationMode::Error && !already_built.empty()) {
        LOG_ERROR_AND_EXIT("Found some of target nodes already in built in the output directory, use --resume or --overwrite");
    }

    // Initialize debug storage if requested (contains .glb meshes)
    std::optional<mesh::storage::Storage> debug_storage = std::nullopt;
    std::mutex debug_storage_mutex;
    if (ctx.options.write_debug_meshes) {
        octree::OpenOptions options;
        options.preferred_extension = ".glb";
        auto debug_result = octree::open_folder(
            ctx.output_storage.base_path().string() + "-debug",
            std::move(options));
        if (!debug_result.has_value()) {
            LOG_ERROR_AND_EXIT(
                "Failed to open debug mesh storage: {}",
                store::describe_error(debug_result.error()));
        }
        debug_storage = std::move(debug_result.value());
    }

    tbb::concurrent_vector<octree::Id> saved_ids;

    ProgressIndicator progress(targets.size());
    auto progress_thread = progress.start_monitoring();

    parallel_foreach(targets, [&](const octree::Id &target) {
        if (already_built.contains(target)) {
            progress.task_finished();
            return;
        }

        const auto dag_ids = find_value(inner_nodes, target).value_or(std::vector<octree::Id>{});
        const auto target_input_ids = find_value(input_sources, target).value_or(std::vector<octree::Id>{});

        auto result = build_node(target, target_input_ids, dag_ids, ctx);
        if (result) {
            const auto save_result = ctx.output_storage.save(target, *result);
            if (save_result) {
                if (debug_storage) {
                    const auto debug_mesh = clustering_to_mesh(result->clustering);
                    const auto debug_save_result = [&] {
                        std::scoped_lock lock(debug_storage_mutex);
                        return debug_storage->save(target, debug_mesh);
                    }();
                    if (!debug_save_result.has_value()) {
                        LOG_ERROR_AND_EXIT(
                            "Failed to save debug mesh for node {}: {}",
                            target,
                            store::describe_error(debug_save_result.error()));
                    }
                }
                saved_ids.push_back(target);
            } else {
                LOG_ERROR(
                    "Failed to save node {}: {}",
                    target,
                    store::describe_error(save_result.error()));
            }
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
std::expected<void, sf::InvalidTopology> build_levels(
    const mesh::storage::IndexedStorage &input_storage,
    dag::storage::IndexedStorage &output_storage,
    const BuildOptions &options,
    const AnyRange<uint32_t> &level_range) {
    const auto validation = sf::validate_index(input_storage.index());
    if (!validation.has_value()) {
        return std::unexpected(validation.error());
    }
    const octree::OddLevelShifted shifted_space = octree::OddLevelShifted::earth();
    const octree::Space space = octree::Space::earth();
    const octree::Id root_node = options.root_node;
    const auto root_bounds = shifted_space.get_node_bounds_with_children(root_node);
    auto input_by_level_result = gather_relevant_input_leaves(input_storage.index(), space, root_bounds);
    if (!input_by_level_result.has_value()) {
        return std::unexpected(input_by_level_result.error());
    }
    auto input_by_level = std::move(input_by_level_result.value());

    // Find the maximum input level that has any nodes
    auto max_input_level_opt = find_max_input_level(input_by_level);
    if (!max_input_level_opt.has_value()) {
        LOG_WARN("No input nodes found for root {}", root_node);
        return {};
    }
    const uint32_t max_input_level = max_input_level_opt.value();

    const Range<uint32_t> valid_range{root_node.level(), max_input_level + 1};
    const Range<uint32_t> range = valid_range.intersect(level_range).to_range(max_input_level + 1);
    if (range.is_empty()) {
        LOG_WARN("Requested level range does not overlap with buildable levels {}-{}", root_node.level(), max_input_level);
        return {};
    }

    // Seed prev_level_built with any already-built nodes one level finer than the first level.
    // TODO: use hierachical lookup here based on root_bounds and IdRect
    std::unordered_set<octree::Id> prev_level_built;
    for (const auto &[id, status] : output_storage.index()) {
        if (id.level() == range.end && status != store::NodeStatus::Virtual) {
            prev_level_built.insert(id);
        }
    }

    BuildContext ctx{input_storage, ThreadSafeStorage(std::move(output_storage)), options, space, shifted_space, root_bounds};

    for (uint32_t level = range.end; level-- > range.start;) {
        prev_level_built = build_level(
            level,
            input_by_level,
            prev_level_built,
            ctx);

        // Persist per level so a finished level can be read back before the whole run completes
        if (const auto result = ctx.output_storage.save_or_create_index(); !result.has_value()) {
            LOG_WARN(
                "Could not save index after level {}: {}",
                level,
                store::describe_error(result.error()));
        }
    }

    output_storage = std::move(ctx.output_storage).release();
    return {};
}

// Builds the complete DAG from input_storage into output_storage.
std::expected<void, sf::InvalidTopology> build_full(
    const mesh::storage::IndexedStorage &input_storage,
    dag::storage::IndexedStorage &output_storage,
    const BuildOptions &options) {
    return build_levels(input_storage, output_storage, options, RangeFull{});
}

} // namespace dag
