#include <filesystem>
#include <queue>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "atlas/atlas.h"
#include "cluster.h"
#include "clusterize.h"
#include "log.h"
#include "mesh/bounds.h"
#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/OddLevelShifted.h"
#include "octree/storage/open.h"
#include "octree/storage/IndexedStorage.h"
#include "partition.h"
#include "simplify.h"
#include "split.h"
#include "utils.h"
#include "validate.h"
#include "encoded.h"
#include "TinyVector.h"
#include "VecHash.h"
#include "error_bounds.h"
#include "compact.h"

inline radix::geometry::Aabb3d calculate_bounds(const Clustering& clustering) {
    return mesh::calculate_bounds(clustering.positions);
}

struct VertexInCluster {
    uint32_t cluster_index;
    uint32_t local_vertex_index;
};

inline std::vector<uint8_t> find_vertices_to_lock(const Clustering& clustering) {
    // Lock every triangle where any vertex is either on the border and near the bounds or shared between clusters.
    const uint32_t cluster_count = clustering.cluster_count();
    const uint32_t vertex_count = clustering.vertex_count();
    std::unordered_set<uint32_t> vertices_to_lock;

    // Find border vertices (vertices that are part of at least one boundary triangle)
    std::unordered_set<uint32_t> boundary_vertices;
    boundary_vertices.reserve(vertex_count);
    std::unordered_set<uint32_t> cluster_boundary_vertices;
    for (const auto &[cluster_index, cluster] : enumerate(clustering.clusters)) {
        cluster_boundary_vertices.clear();
        mesh::find_boundary_vertices(cluster.local_triangles, cluster_boundary_vertices);
        
        for (const uint32_t local_vertex_index : cluster_boundary_vertices) {
            const uint32_t global_vertex_index = cluster.vertex_indices[local_vertex_index];
            boundary_vertices.insert(global_vertex_index);
        }
    }

    // Calculate safe bounds excluding the outer boundary of the mesh
    const radix::geometry::Aabb3d bounds = calculate_bounds(clustering);
    const glm::dvec3 center = bounds.centre();
    const glm::dvec3 extents = bounds.size() / 2.0;
    const glm::dvec3 unlocked_extents = extents * 0.99;
    const radix::geometry::Aabb3d unlocked_bounds(center - unlocked_extents, center + unlocked_extents);

    // Lock vertices outside safe bounds that are on the boundary
    for (const uint32_t vertex_index : boundary_vertices) {
        // Lock only vertices outside the unlocked bounds
        const glm::dvec3& position = clustering.positions[vertex_index];
        if (!unlocked_bounds.contains(position)) {
            vertices_to_lock.insert(vertex_index);
        }
    }

    // Find all vertices shared between at least 2 clusters
    std::vector<TinyVector<VertexInCluster>> cluster_membership(vertex_count);
    for (uint32_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        for (const auto [local_vertex_index, vertex_index] : enumerate(cluster.vertex_indices)) {
            TinyVector<VertexInCluster> &membership = cluster_membership[vertex_index];
            membership.emplace_back(cluster_index, local_vertex_index);
        }
    }
    for (const auto& [vertex_index, membership] : enumerate(cluster_membership)) {
        const size_t num_clusters = membership.size();
        DEBUG_ASSERT(num_clusters > 0);
        if (membership.size() == 1) {
            continue;
        }

        // Make sure the vertex is not just duplicated in a single cluster
        const uint32_t first_cluster_index = membership[0].cluster_index;
        bool all_from_same = true;
        for (uint32_t i = 1; i < num_clusters; i++) {
            if (membership[i].cluster_index != first_cluster_index) {
                all_from_same = false;
                break;
            }
        }
        if (all_from_same) {
            continue;
        }

        // Encountered a shared vertex
        // Get global vertex
        const Cluster &first_cluster = clustering.clusters[membership[0].cluster_index];
        const uint32_t global_vertex_index = first_cluster.vertex_indices[membership[0].local_vertex_index];
        for (uint32_t i = 1; i < num_clusters; i++) {
            const auto [cluster_index, local_vertex_index] = membership[i];
            const Cluster &cluster = clustering.clusters[cluster_index];
            const uint32_t other_global_vertex_index = cluster.vertex_indices[local_vertex_index];
            DEBUG_ASSERT(global_vertex_index == other_global_vertex_index);
        }

        // Check if on the boundary
        if (!boundary_vertices.contains(global_vertex_index)) {
            continue;
        }

        // If its also on the boundary mark as locked
        vertices_to_lock.insert(global_vertex_index);
    }

    // Allocate vertex lock buffer
    std::vector<uint8_t> vertex_lock(vertex_count, VertexLock::UNLOCKED);
    for (const uint32_t vertex_index : vertices_to_lock) {
        vertex_lock[vertex_index] = VertexLock::LOCKED;
    }

    /*
    // Create cluster-local vertex to triangle mapping
    std::vector<std::vector<std::vector<uint32_t>>> cluster_vertex_to_triangles(cluster_count);
    for (const uint32_t cluster_index : range(cluster_count)) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        auto& vertex_to_triangles = cluster_vertex_to_triangles[cluster_index];
        vertex_to_triangles = mesh::create_vertex_to_triangle_mapping(cluster.local_triangles);
    }

    // Find all triangles that reference locked vertices and lock all their vertices
    // We need to lock the whole triangles to avoid changing the border between clusters, since otherwise we might end up with gaps in the mesh after simplification.
    for (const uint32_t vertex_index : vertices_to_lock) {
        const std::span<const VertexInCluster> membership = cluster_membership[vertex_index];
        for (const auto [cluster_index, local_vertex_index] : membership) {
            const Cluster &cluster = clustering.clusters[cluster_index];
            const std::vector<uint32_t> &triangles_of_vertex = cluster_vertex_to_triangles[cluster_index][local_vertex_index];
            for (const uint32_t triangle_index : triangles_of_vertex) {
                const glm::uvec3 &triangle = cluster.local_triangles[triangle_index];
                const glm::uvec3 global_triangle(
                    cluster.vertex_indices[triangle.x],
                    cluster.vertex_indices[triangle.y],
                    cluster.vertex_indices[triangle.z]);
                vertex_lock[global_triangle.x] = VertexLock::LOCKED;
                vertex_lock[global_triangle.y] = VertexLock::LOCKED;
                vertex_lock[global_triangle.z] = VertexLock::LOCKED;
            }
        }
    }
    */

    return vertex_lock;
}

class ClusteringStageLogger {
public:
    explicit ClusteringStageLogger(const Clustering &clustering)
        : _last_clusters(clustering.cluster_count()),
          _last_vertices(clustering.vertex_count()) {}

    void log_initial() {
        LOG_INFO(
            "stage=initial clusters={} vertices={}",
            this->_last_clusters,
            this->_last_vertices);
    }

    void log(const char *stage, const Clustering &clustering) {
        const uint32_t clusters = clustering.cluster_count();
        const uint32_t vertices = clustering.vertex_count();

        LOG_INFO(
            "stage={} clusters={}->{} vertices={}->{}",
            stage,
            this->_last_clusters,
            clusters,
            this->_last_vertices,
            vertices);

        this->_last_clusters = clusters;
        this->_last_vertices = vertices;
    }
private:
    uint32_t _last_clusters;
    uint32_t _last_vertices;
};
cv::Mat make_lock_debug_texture() {
    // 1 row, 2 columns, RGBA
    cv::Mat tex(1, 2, CV_8UC4);

    tex.at<cv::Vec4b>(0, 0) = cv::Vec4b(255, 255, 255, 255); // white
    tex.at<cv::Vec4b>(0, 1) = cv::Vec4b(255, 0, 0, 255);     // red

    return tex;
}
Clustering make_vertex_lock_debug_clustering(
    const Clustering &clustering,
    const std::vector<uint8_t> &vertex_lock) {
    Clustering debug = clustering;

    debug.textures.clear();
    debug.textures.push_back(make_lock_debug_texture());

    for (Cluster &cluster : debug.clusters) {
        cluster.texture_id = 0;
        cluster.uvs.resize(cluster.vertex_indices.size());

        for (size_t local_vi = 0; local_vi < cluster.vertex_indices.size(); ++local_vi) {
            const uint32_t global_vi = cluster.vertex_indices[local_vi];

            const bool locked =
                global_vi < vertex_lock.size() && vertex_lock[global_vi] != 0;

            // Sample texel center:
            // x = 0.25 -> white texel
            // x = 0.75 -> red texel
            cluster.uvs[local_vi] = locked
                                        ? glm::dvec2(0.75, 0.5)
                                        : glm::dvec2(0.25, 0.5);
        }
    }

    return debug;
}

int main2(int argc, char **argv) {
    USE(argc);
    USE(argv);

    const std::filesystem::path input =
        // "/home/user/master/meshes/innenstadt8/13/6478/4796/6856.glb";
        // "/home/user/master/meshes/innenstadt10/15/25917/19184/27423.glb";
        // "/home/user/master/meshes/innenstadt10/15/25914/19185/27426.glb";
        "/home/user/master/meshes/innenstadt11/15/25917/19185/27422.glb";
    const std::filesystem::path out_dir =
            "/home/user/master/meshes/lod_tree";
    std::filesystem::create_directories(out_dir);

    auto mesh = mesh::io::load_from_path(input).value();

    // Perform initial clustering
    auto clustering = clusterize(mesh);
    validate(clustering);

    ClusteringStageLogger stage_logger(clustering);
    stage_logger.log_initial();

    for (int level = 18; level >= 0; level--) {
        // Partition clusters into groups
        clustering = partition(clustering, PartitionOptions{.clusters_per_partition = 8});
        validate(clustering);
        stage_logger.log("partitioned", clustering);

        // Find vertices to lock
        const std::vector<uint8_t> vertex_lock = find_vertices_to_lock(clustering);

        // Simplify each cluster
        clustering = simplify(clustering, SimplifyOptions{
                                              .target_ratio = 0.25,
                                              .vertex_lock = VertexLock::mask(vertex_lock)
                                          });
        validate(clustering);
        remove_unused_vertices_inplace(clustering);
        validate(clustering);
        stage_logger.log("simplified", clustering);

        // Output to file
        mesh::io::save_to_path(
            clustering_to_mesh(clustering),
            (out_dir / ("level_" + std::to_string(level) + ".glb")).string());
        if (clustering.clusters.size() == 1) {
            LOG_INFO("Stopping at single remaining cluster");
            break;
        }

        // Split each cluster into roughly 4 parts
        clustering = clusterize(clustering).clustering;
        validate(clustering);
        stage_logger.log("reclustered", clustering);

        return 0;
    }

    return 0;
}

template <typename T>
T quantize(const T x, const T epsilon) {
    //  x - remainder(x, epsilon) is not idempotent
    return std::round(x / epsilon) * epsilon;
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> quantize(const glm::vec<n_dims, T> &v, const T epsilon) {
    glm::vec<n_dims, T> result;
    for (glm::length_t i = 0; i < n_dims; i++) {
        result[i] = quantize(v[i], epsilon);
    }
    return result;
}

#include "mesh/triangle_compare.h"
Clustering merge_clusterings(const std::vector<Clustering> &clusterings) {
    if (clusterings.size() == 1) {
        return clusterings[0];
    }

    // Create new position buffer
    const uint32_t vertex_count_bound = sum(clusterings, [&](const auto& clustering) { return clustering.vertex_count(); });
    const double epsilon = metres_per_pixel(14) / 1000;
    std::unordered_map<glm::dvec3, uint32_t, DVec3Hash> vertex_remap;
    vertex_remap.reserve(vertex_count_bound);
    std::vector<glm::dvec3> new_positions;
    new_positions.reserve(vertex_count_bound);
    for (const Clustering &clustering : clusterings) {
        for (const glm::dvec3& position : clustering.positions) {
            const glm::dvec3 quantized = quantize(position, epsilon);
            if (!vertex_remap.contains(quantized)) {
                // insert new mapping
                const uint32_t new_index = new_positions.size();
                vertex_remap.emplace(quantized, new_index);
                new_positions.push_back(position);
            }
        }
    }

    Clustering merged;
    merged.positions = new_positions;

    for (const Clustering &clustering : clusterings) {
        // Merge textures
        const uint32_t texture_offset = merged.textures.size();
        for (const cv::Mat &texture : clustering.textures) {
            merged.textures.push_back(texture);
        }

        // Merge clusters with adjusted indices
        for (const Cluster &cluster : clustering.clusters) {
            Cluster new_cluster;
            new_cluster.local_triangles = cluster.local_triangles;
            new_cluster.uvs = cluster.uvs;

            new_cluster.texture_id = cluster.texture_id + texture_offset;

            new_cluster.vertex_indices.reserve(cluster.vertex_count());
            for (const uint32_t vertex_index : cluster.vertex_indices) {
                const glm::dvec3 &position = clustering.positions[vertex_index];
                const glm::dvec3 quantized = quantize(position, epsilon);
                const uint32_t new_index = vertex_remap[quantized];
                new_cluster.vertex_indices.push_back(new_index);
            }

            merged.clusters.push_back(std::move(new_cluster));
        }
    }

    // Remove degenerate triangles
    std::vector<uint32_t> triangles_to_remove;
    for (Cluster& cluster : merged.clusters) {
        const size_t removed = std::erase_if(cluster.local_triangles, [&](const auto &local_triangle) {
            const glm::uvec3 global_triangle(
                cluster.vertex_indices[local_triangle.x],
                cluster.vertex_indices[local_triangle.y],
                cluster.vertex_indices[local_triangle.z]);
            return mesh::is_degenerate(global_triangle);
        });

        if (removed > 0) {
            compact_cluster(cluster);
        }
    }

    validate(merged);
    return merged;
}

Clustering run_pipeline(Clustering clustering) {
    ClusteringStageLogger stage_logger(clustering);
    stage_logger.log_initial();

    // Partition clusters into groups
    const Partitioning partitioning = create_partitioning(clustering, options);
    clustering = apply_partitioning(clustering, partitioning);

    clustering = partition(clustering, PartitionOptions{.clusters_per_partition = 8});
    validate(clustering);
    stage_logger.log("partitioned", clustering);

    // Trim textures
    trim_textures_inplace(clustering);

    // Find vertices to lock
    const std::vector<uint8_t> vertex_lock = find_vertices_to_lock(clustering);
    // const Clustering debug_clustering =
    //     make_vertex_lock_debug_clustering(clustering, vertex_lock);
    // mesh::io::save_to_path(
    //     clustering_to_mesh(debug_clustering),
    //    (out_dir / ("level_" + std::to_string(level) + "_locks.glb")).string());

    // Simplify each cluster
    clustering = simplify(clustering, SimplifyOptions{
                                         .target_ratio = 0.25,
                                         .vertex_lock = VertexLock::mask(vertex_lock)
                                    });
    validate(clustering);
    remove_unused_vertices_inplace(clustering);
    validate(clustering);
    stage_logger.log("simplified", clustering);

    // Split each cluster into roughly 4 parts
    clustering = clusterize(clustering).clustering;
    validate(clustering);
    stage_logger.log("reclustered", clustering);



    return clustering;
}

struct DagNode {
    octree::Id id;
    Clustering clustering;
};

inline std::filesystem::path get_dag_node_path(const octree::Id &id) {
    // return fmt::format("/home/user/master/meshes/lod_tree4/{}/{}/{}/{}.glb", id.level(), id.x(), id.y(), id.z());
    return fmt::format("/home/user/master/meshes/lod_tree4/{}/{}-{}-{}.glb", id.level(), id.x(), id.y(), id.z());
}

std::optional<DagNode> load_dag_node(const octree::Id &id) {
    const std::filesystem::path path = get_dag_node_path(id);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }
    const auto result = load_clustering(path);
    if (!result) {
        LOG_ERROR("Failed to load node from {}: {}", path, result.error());
        return std::nullopt;
    }
    const Clustering clustering = result.value();
    return DagNode{id, clustering};
}

void save_dag_node(const DagNode &node) {
    const std::filesystem::path path = get_dag_node_path(node.id);
    const auto result = save_clustering(node.clustering, path);
    if (!result) {
        LOG_ERROR("Failed to save node to {}: {}", path, result.error());
    }

    // Save mesh next to it
    std::filesystem::path mesh_path = path;
    mesh_path.replace_extension(".glb");
    const auto mesh_result = mesh::io::save_to_path(clustering_to_mesh(node.clustering), mesh_path.string());
    if (!mesh_result) {
        LOG_ERROR("Failed to save mesh to {}: {}", mesh_path, mesh_result.error());
    }
}

std::optional<Clustering> load_and_clusterize_mesh(const octree::Storage& storage, const octree::Id &id) {
    const auto result = storage.read_node(id);
    if (!result) {
        if (result.error() != mesh::io::LoadMeshErrorKind::FileNotFound) {
            LOG_ERROR("Failed to read node {}: {}", id, result.error());
        }
        return std::nullopt;
    }

    const mesh::Simple& mesh = result.value();
    // TODO: remove this
    if (!mesh.has_texture() || !mesh.has_uvs()) {
        LOG_WARN("Mesh {} has no texture, skipping", id);
        return std::nullopt;
    }

    LOG_DEBUG("Clustering mesh for node {} with {} vertices and {} triangles", id, mesh.vertex_count(), mesh.face_count());
    return clusterize(mesh);
}

int main(int argc, char **argv) {
    Log::init(spdlog::level::trace);

    const std::filesystem::path input_dir = "/home/user/master/meshes/innenstadt11";

    // Load index from input directory
    const octree::IndexedStorage storage = octree::open_folder_indexed(input_dir);
    const octree::IndexMap& index_map = storage.index();

    // Init nodes by level based on input storage.
    std::vector<std::unordered_set<octree::Id>> nodes_by_level(MAX_LEVEL);
    for (const auto &[id, entry] : index_map) {
        nodes_by_level[id.level()].insert(id);
    }

    // Find highest zoom level
    uint32_t max_zoom = 0;
    for (const auto& [level, nodes] : enumerate(nodes_by_level)) {
        if (!nodes.empty()) {
            max_zoom = level;
        }
    }

    const octree::OddLevelShifted space = octree::OddLevelShifted::earth();
    for (uint32_t zoom = max_zoom; zoom > 0; zoom--) {
        LOG_DEBUG("Zoom level {} has {} nodes", zoom, nodes_by_level[zoom].size());

        // Find all parent nodes that intersect with at least one node at the current zoom level
        std::unordered_set<octree::Id> relevant_parents;
        for (const auto &id : nodes_by_level[max_zoom]) {
            const std::vector<octree::Id> intersecting_parents = space.get_intersecting_nodes_on_previous_level(id);
            relevant_parents.insert(intersecting_parents.begin(), intersecting_parents.end());
        }

        LOG_INFO("Building {} nodes for zoom level {}", relevant_parents.size(), zoom - 1);

        // For each relevant parent node, run the pipeline.
        for (const octree::Id &parent_id : relevant_parents) {
            LOG_DEBUG("Processing node {}", parent_id);

            // Find all intersecting child nodes
            const std::vector<octree::Id> relevant_ids = space.get_intersecting_nodes_on_next_level(parent_id);
            LOG_DEBUG("Node {} has {} intersecting children", parent_id, relevant_ids.size());

            // Load or build child clusterings
            std::vector<Clustering> relevant_clusterings;
            for (const octree::Id &child_id : relevant_ids) {
                if (const auto result = load_dag_node(child_id)) {
                    relevant_clusterings.push_back(std::move(result.value().clustering));
                }
                if (const auto result = load_and_clusterize_mesh(storage, child_id)) {
                    relevant_clusterings.push_back(std::move(result.value()));
                }
            }

            if (relevant_clusterings.empty()) {
                LOG_WARN("No valid children found for node {}, skipping", parent_id);
                continue;
            }

            LOG_DEBUG("Merging {} clusterings for node {}", relevant_clusterings.size(), parent_id);
            const Clustering merged_clustering = merge_clusterings(relevant_clusterings);
            LOG_DEBUG("Running pipeline for node {} with merged clustering of {} vertices and {} clusters", parent_id, merged_clustering.vertex_count(), merged_clustering.cluster_count());
            const Clustering final_clustering = run_pipeline(merged_clustering);
            LOG_INFO("Finished node {} with final clustering of {} vertices and {} clusters", parent_id, final_clustering.vertex_count(), final_clustering.cluster_count());
            nodes_by_level[parent_id.level()].insert(parent_id);

            // Output to file
            save_dag_node(DagNode{parent_id, final_clustering});
        }
    }
}
