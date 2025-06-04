#include <numeric>
#include <unordered_map>

#include "mesh/convert.h"
#include "log.h"
#include "merge.h"
#include "mesh/SimpleMesh.h"
#include "mesh/utils.h"
#include "validate.h"

using namespace merge;

template <typename T>
static T max_component(const glm::tvec3<T> &vector) {
    return glm::max(glm::max(vector.x, vector.y), vector.z);
}

template <typename T>
static T min_component(const glm::tvec3<T> &vector) {
    return glm::min(glm::min(vector.x, vector.y), vector.z);
}

template <typename T>
class Grid3d {
public:
    Grid3d(const glm::dvec3 origin, const glm::dvec3 size, const glm::uvec3 divisions)
        : origin(origin), size(size), divisions(divisions) {
        grid_data.resize(divisions.x * divisions.y * divisions.z);
    }

    struct GridCellItem {
        glm::dvec3 point;
        T value;
    };

    struct GridCell {
        std::vector<GridCellItem> items;
    };

    std::optional<std::reference_wrapper<T>> find(const glm::dvec3 point, const double epsilon = 0.1f) {
        assert(epsilon > 0);

        if (!is_in_bounds(point)) {
            return std::nullopt;
        }

        const glm::uvec3 grid_index = this->calculate_grid_index(point);
        const glm::dvec3 cell_size = this->cell_size();
        const double max_cell_size = max_component(cell_size);
        int cell_radius = std::ceil(epsilon / max_cell_size);
        if (cell_radius > 1) {
            LOG_WARN("Grid lookup epsilon ({:g}) is high compared to cell size ({:g}, {:g}, {:g}) resulting in cell radius of {}",
                     epsilon, cell_size.x, cell_size.y, cell_size.z, cell_radius);
        }

        const glm::dvec3 distance_from_contained_cell_center = point - cell_size * (glm::dvec3(grid_index) + glm::dvec3(0.5));
        if (glm::all(glm::lessThan(distance_from_contained_cell_center + epsilon, cell_size))) {
            cell_radius = 0;
        }

        std::optional<std::reference_wrapper<T>> closest_value;
        double closest_distance = std::numeric_limits<double>::infinity();

        for (int dx = -cell_radius; dx <= cell_radius; dx++) {
            for (int dy = -cell_radius; dy <= cell_radius; dy++) {
                for (int dz = -cell_radius; dz <= cell_radius; dz++) {
                    glm::ivec3 _neighbor_index = glm::ivec3(grid_index) + glm::ivec3(dx, dy, dz);
                    if (!this->is_valid_grid_index(_neighbor_index)) {
                        continue;
                    }

                    glm::uvec3 neighbor_index(_neighbor_index);
                    const size_t cell_index = this->calculate_cell_index(neighbor_index);
                    GridCell &cell = this->grid_data[cell_index];

                    for (GridCellItem &item : cell.items) {
                        const double distance = glm::distance(point, item.point);
                        if (distance < epsilon && distance < closest_distance) {
                            closest_value = item.value;
                            closest_distance = distance;
                        }
                    }
                }
            }
        }

        return closest_value;
    }

    std::optional<std::reference_wrapper<const T>> find(const glm::dvec3 point, const double epsilon = 0.1f) const {
        return const_cast<std::optional<std::reference_wrapper<const T>>>(
            const_cast<const Grid3d *>(this)->find(point, epsilon));
    }

    void insert(const glm::dvec3 point, const T value) {
        if (!is_in_bounds(point)) {
            return;
        }

        const size_t cell_index = this->calculate_cell_index(point);

        const GridCellItem item{
            .point = point,
            .value = value};
        GridCell &cell = grid_data[cell_index];
        cell.items.push_back(item);
        this->overall_point_count += 1;

        if (cell.items.size() >= 1000) {
            LOG_WARN("Large number of points inside single grid cell: {} ({} overall)", cell.items.size(), this->overall_point_count);
        }
    }

    bool is_in_bounds(const glm::dvec3 point) const {
        const glm::dvec3 max_bounds = origin + size;
        return glm::all(glm::greaterThanEqual(point, origin)) && glm::all(glm::lessThanEqual(point, max_bounds));
    }

    glm::dvec3 cell_size() const {
        return this->size / glm::dvec3(this->divisions);
    }

    glm::uvec3 calculate_grid_index(const glm::dvec3 point) const {
        return glm::uvec3((point - this->origin) / this->cell_size());
    }

    size_t calculate_cell_index(const glm::dvec3 point) const {
        const glm::uvec3 grid_index = this->calculate_grid_index(point);
        return this->calculate_cell_index(grid_index);
    }

    size_t calculate_cell_index(const glm::uvec3 grid_index) const {
        return grid_index.x + grid_index.y * divisions.x + grid_index.z * divisions.x * divisions.y;
    }

    bool is_point_inside_cell(const glm::dvec3 &point, const glm::uvec3 &grid_index) const {
        const glm::dvec3 cell_size = this->cell_size();
        const glm::dvec3 cell_min = this->origin + glm::dvec3(grid_index) * cell_size;
        const glm::dvec3 cell_max = cell_min + cell_size;

        return glm::all(glm::greaterThanEqual(point, cell_min)) && glm::all(glm::lessThanEqual(point, cell_max));
    }

    bool is_valid_grid_index(const glm::ivec3 &grid_index) const {
        return grid_index.x >= 0 && grid_index.y >= 0 && grid_index.z >= 0 && this->is_valid_grid_index(glm::uvec3(grid_index));
    }
    bool is_valid_grid_index(const glm::uvec3 &grid_index) const {
        return grid_index.x < divisions.x && grid_index.y < divisions.y && grid_index.z < divisions.z;
    }

    const GridCell &cell(const glm::uvec3 &grid_index) const {
        return this->cell(calculate_cell_index(grid_index));
    }
    const GridCell &cell(const size_t cell_index) const {
        return this->grid_data[cell_index];
    }

    const std::span<const GridCell> cells() const {
        return this->grid_data;
    }

    const glm::dvec3 origin;
    const glm::dvec3 size;
    const glm::uvec3 divisions;

private:
    size_t overall_point_count;
    std::vector<GridCell> grid_data;
};

static radix::geometry::Aabb3d pad_bounds(const radix::geometry::Aabb3d &bounds, const double percentage) {
    const glm::dvec3 bounds_padding = bounds.size() * percentage;
    const radix::geometry::Aabb3d padded_bounds(bounds.min - bounds_padding, bounds.max + bounds_padding);
    return padded_bounds;
}

template <typename T>
static Grid3d<T> _construct_grid_for_meshes(const radix::geometry::Aabb3d &bounds, const size_t vertex_count) {
    const radix::geometry::Aabb3d padded_bounds = pad_bounds(bounds, 0.01);

    const double max_extends = max_component(padded_bounds.size());
    const glm::dvec3 relative_extends = padded_bounds.size() / max_extends;
    const glm::uvec3 grid_divisions = glm::max(glm::uvec3(2 * std::cbrt(vertex_count) * relative_extends), glm::uvec3(1));
    Grid3d<T> grid(padded_bounds.min, padded_bounds.size(), grid_divisions);

    return grid;
}

template <typename T>
static Grid3d<T> construct_grid_for_mesh(const SimpleMesh &mesh) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(mesh);
    const size_t vertex_count = mesh.vertex_count();
    return _construct_grid_for_meshes<T>(bounds, vertex_count);
}

template <typename T>
static Grid3d<T> construct_grid_for_meshes(const std::span<const SimpleMesh> meshes) {
    const radix::geometry::Aabb3d bounds = calculate_bounds(meshes);
    const size_t maximal_merged_mesh_size = std::transform_reduce(
        meshes.begin(), meshes.end(), 0, [](const size_t a, const size_t b) { return a + b; }, [](const SimpleMesh &mesh) { return mesh.vertex_count(); });
    return _construct_grid_for_meshes<T>(bounds, maximal_merged_mesh_size);
}

static double estimate_min_vertex_separation_between_meshes_after_merge(const Grid3d<VertexId> &grid, const VertexMapping &mapping) {
    double min_squared_distance = std::numeric_limits<double>::infinity();
    for (const Grid3d<VertexId>::GridCell &cell : grid.cells()) {
        for (auto first = cell.items.begin(); first != cell.items.end(); ++first) {
            for (auto second = first + 1; second != cell.items.end(); ++second) {
                const size_t mesh1 = first->value.mesh_index;
                const size_t mesh2 = second->value.mesh_index;

                if (mesh1 == mesh2) {
                    continue;
                }

                const glm::dvec3 &point1 = first->point;
                const glm::dvec3 &point2 = second->point;

                const double squared_distance = glm::distance2(point1, point2);
                min_squared_distance = std::min(min_squared_distance, squared_distance);
            }
        }
    }

    return std::sqrt(min_squared_distance);
}

static double estimate_min_vertex_separation_between_meshes_after_merge(const std::span<const SimpleMesh> meshes, const VertexMapping &mapping) {
    Grid3d<VertexId> grid = construct_grid_for_meshes<VertexId>(meshes);

    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const glm::dvec3 &position = mesh.positions[vertex_index];
            grid.insert(position, VertexId {mesh_index, vertex_index});
        }
    }

    double min_squared_distance = std::numeric_limits<double>::infinity();
    for (const Grid3d<VertexId>::GridCell &cell : grid.cells()) {
        for (auto first = cell.items.begin(); first != cell.items.end(); ++first) {
            for (auto second = first + 1; second != cell.items.end(); ++second) {
                const size_t mesh1 = first->value.mesh_index;
                const size_t mesh2 = second->value.mesh_index;

                if (mesh1 == mesh2) {
                    // Both vertices from the same mesh
                    continue;
                }

                const size_t mapped1 = mapping.map(first->value);
                const size_t mapped2 = mapping.map(second->value);
                if (mapped1 == mapped2) {
                    // Vertices were already merged
                    continue;
                }

                const glm::dvec3 &point1 = first->point;
                const glm::dvec3 &point2 = second->point;
                const double squared_distance = glm::distance2(point1, point2);
                min_squared_distance = std::min(min_squared_distance, squared_distance);
            }
        }
    }

    return std::sqrt(min_squared_distance);
}

static double estimate_average_vertex_separation(const SimpleMesh &mesh, const size_t sample_size = 1000) {
    std::vector<glm::uvec3> triangles = mesh.triangles;
    std::random_shuffle(triangles.begin(), triangles.end());

    size_t count = std::min(triangles.size(), sample_size);
    double average_distance = 0;
    for (size_t i = 0; i < count; i++) {
        const glm::uvec3 &triangle = triangles[i];

        const size_t first_index_in_triangle = i % triangle.length();
        const size_t first_index = triangle[first_index_in_triangle];
        const size_t second_index = triangle[(first_index_in_triangle + 1) % triangle.length()];

        const double distance2 = glm::distance(mesh.positions[first_index], mesh.positions[second_index]);
        average_distance += distance2 / count;
    }

    return std::sqrt(average_distance);
}

static double estimate_average_vertex_seperation(const SimpleMesh &mesh, const size_t sample_size = 1000) {
    std::vector<glm::uvec3> triangles = mesh.triangles;
    std::random_shuffle(triangles.begin(), triangles.end());

    size_t count = std::min(triangles.size(), sample_size);
    double average_distance = 0;
    for (size_t i = 0; i < count; i++) {
        const glm::uvec3 &triangle = triangles[i];

        const size_t first_index_in_triangle = i % triangle.length();
        const size_t first_index = triangle[first_index_in_triangle];
        const size_t second_index = triangle[(first_index_in_triangle + 1) % triangle.length()];

        const double distance2 = glm::distance(mesh.positions[first_index], mesh.positions[second_index]);
        average_distance += distance2 / count;
    }

    return std::sqrt(average_distance);
}

static double estimate_min_edge_length(const SimpleMesh &mesh) {
    std::vector<glm::uvec3> triangles = mesh.triangles;
    std::random_shuffle(triangles.begin(), triangles.end());

    const size_t count = std::min(triangles.size(), std::max<size_t>(1000, static_cast<size_t>(triangles.size() * 0.01)));
    double min_edge_length = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < count; i++) {
        const glm::uvec3 &triangle = triangles[i];

        const size_t first_index_in_triangle = i % triangle.length();
        const size_t first_index = triangle[first_index_in_triangle];
        const size_t second_index = triangle[(first_index_in_triangle + 1) % triangle.length()];

        const double distance2 = glm::distance2(mesh.positions[first_index], mesh.positions[second_index]);
        min_edge_length = std::min(distance2, min_edge_length);
    }

    return std::sqrt(min_edge_length);
}

static double measure_min_edge_length(const SimpleMesh &mesh) {
    const auto min = [](const auto a, const auto b) { return std::min(a, b); };

    const double min_squared_edge_length = std::transform_reduce(
        mesh.triangles.begin(), mesh.triangles.end(),
        std::numeric_limits<double>::infinity(), min,
        [&](const glm::uvec3 &triangle) {
            const std::array<glm::dvec3, 3> positions{
                mesh.positions[triangle[0]],
                mesh.positions[triangle[1]],
                mesh.positions[triangle[2]],
            };

            return std::transform_reduce(
                positions.begin(), positions.end(),
                std::numeric_limits<double>::infinity(), min,
                [&](const glm::dvec3 &v1) {
                    const size_t i = std::distance(positions.begin(), &v1);
                    const glm::dvec3 v2 = positions[(i + 1) % positions.size()];
                    return glm::distance2(v1, v2);
                });
        });

    return std::sqrt(min_squared_edge_length);
}

class UnionFind {
    using IndexType = size_t;
    using SizeType = size_t;

    std::vector<IndexType> parents;
    std::vector<SizeType> sizes;

public:
    UnionFind(const size_t size) {
        this->parents.resize(size);
        std::iota(this->parents.begin(), this->parents.end(), 0);

        this->sizes.resize(size, 1);
    }

    size_t find(const IndexType x) {
        IndexType &x_parent = this->parents[x];
        if (x_parent != x) {
            const IndexType x_rep = find(x_parent);
            x_parent = x_rep;
        }
        return x_parent /* this is x_rep */;
    }

    void make_union(const IndexType x, const IndexType y) {
        const IndexType x_rep = find(x);
        const IndexType y_rep = find(y);

        if (x_rep == y_rep) {
            return;
        }

        const SizeType x_size = this->sizes[x_rep];
        const SizeType y_size = this->sizes[y_rep];
        if (x_size < y_size) {
            this->parents[x_rep] = y_rep;
            this->sizes[y_rep] += this->sizes[x_rep];
        } else {
            this->parents[y_rep] = x_rep;
            this->sizes[x_rep] += this->sizes[y_rep];
        }
    }

    size_t size() {
        return this->parents.size();
    }

    bool is_joint() {
        return std::find(this->sizes.begin(), this->sizes.end(), this->size()) != this->sizes.end();
    }

    bool is_disjoint() {
        return !this->is_joint();
    }
};

static bool are_all_meshes_merged(const VertexMapping &mapping) {
    UnionFind union_find(mapping.mesh_count());

    const size_t maximal_merged_mesh_index = mapping.find_max_merged_index();

    std::unordered_set<size_t> observed_sources;
    observed_sources.reserve(mapping.mesh_count());
    for (size_t vertex_index = 0; vertex_index < maximal_merged_mesh_index; vertex_index++) {
        observed_sources.clear();
        for (size_t mesh_index = 0; mesh_index < mapping.mesh_count(); mesh_index++) {
            if (auto opt = mapping.map_inverse(mesh_index, vertex_index); opt.has_value()) {
                observed_sources.insert(mesh_index);
                if (observed_sources.size() > 1) {
                    for (size_t observed_source : observed_sources) {
                        if (observed_source == mesh_index) {
                            continue;
                        }

                        union_find.make_union(observed_source, mesh_index);
                    }
                }
            }
        }
    }

    return union_find.is_joint();
}

VertexMapping merge::create_merge_mapping(const std::span<const SimpleMesh> meshes) {
    LOG_DEBUG("Finding shared vertices between {} meshes (epsilon=auto)", meshes.size());

    const double min_edge_length = std::transform_reduce(
        meshes.begin(),
        meshes.end(),
        std::numeric_limits<double>::infinity(),
        [](const double a, const double b) { return std::min(a, b); },
        measure_min_edge_length); // XXX: use approximate_min_edge_length in Release?
    double distance_epsilon = min_edge_length / 1000;
    LOG_TRACE("Starting with distance epsilon of {:g}", distance_epsilon);

    VertexMapping mapping;
    bool success = false;
    for (size_t i = 0; i < 10; i++) {
        mapping = create_merge_mapping(meshes, distance_epsilon);

        if (are_all_meshes_merged(mapping)) {
            LOG_TRACE("Found distance epsilon that connects all meshes");

            const double min_vertex_separation_after_merge = estimate_min_vertex_separation_between_meshes_after_merge(meshes, mapping);
            if (min_vertex_separation_after_merge > min_edge_length / 2) {
                LOG_TRACE("Found vertices in merged mesh that are much closer than in the source meshes, suggesting and incomplete merge");
                continue;
            }

                success = true;
            break;
        }
        distance_epsilon *= 10;
        LOG_TRACE("Increasing distance epsilon to {:g}", distance_epsilon);
    }

    if (!success) {
        LOG_TRACE("Failed to find appropriate distance epsilon");
    }

    return mapping;
}

static bool are_all_bounds_connected(const std::span<const SimpleMesh> meshes) {
    if (meshes.size() <= 1) {
        return true;
    }

    std::vector<radix::geometry::Aabb3d> mesh_bounds;
    mesh_bounds.reserve(meshes.size());
    std::transform(meshes.begin(), meshes.end(),
                   std::back_inserter(mesh_bounds),
                   [](const SimpleMesh &mesh) { return pad_bounds(calculate_bounds(mesh), 0.01); });
    for (size_t i = 0; i < mesh_bounds.size(); i++) {
        bool intersect_any_other = false;
        for (size_t j = 0; j < mesh_bounds.size(); j++) {
            if (i == j) {
                continue;
            }

            if (radix::geometry::intersect(mesh_bounds[i], mesh_bounds[j])) {
                intersect_any_other = true;
                break;
            }
        }
        if (!intersect_any_other) {
            LOG_WARN("Mesh at index {} is not close to any other mesh", i);
            return false;
        }
    }

    return true;
}

VertexMapping merge::create_merge_mapping(const std::span<const SimpleMesh> meshes, double distance_epsilon) {
    if (meshes.empty()) {
        return {};
    }
    if (meshes.size() == 1) {
        return VertexMapping::identity(meshes[0].vertex_count());
    }

    LOG_TRACE("Finding shared vertices between {} meshes (epsilon={:g})", meshes.size(), distance_epsilon);

    std::vector<size_t> mesh_sizes;
    mesh_sizes.reserve(meshes.size());
    std::transform(meshes.begin(), meshes.end(),
                   std::back_inserter(mesh_sizes),
                   [](const SimpleMesh &mesh) { return mesh.vertex_count(); });
    const size_t maximal_merged_mesh_size = std::accumulate(mesh_sizes.begin(), mesh_sizes.end(), 0);

    VertexMapping mapping;
    mapping.init(mesh_sizes);

    // Create a grid as a spatial acceleration structure, its size and granularity depends on the meshes.
    // Note that this is not very performant if the meshes are actually disconnected.
    Grid3d<VertexId> grid = construct_grid_for_meshes<VertexId>(meshes);

    size_t unique_vertices = 0;
    bool has_warned = false;
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const glm::dvec3 &position = mesh.positions[vertex_index];
            const VertexId current_vertex{
                .mesh_index = mesh_index,
                .vertex_index = vertex_index};

            const std::optional<std::reference_wrapper<VertexId>> other_vertex = grid.find(position, distance_epsilon);
            if (!has_warned && other_vertex.has_value() && other_vertex->get().mesh_index == current_vertex.mesh_index) {
                LOG_WARN("Merge distance epsilon is large and would perform intra-mesh merges");
                has_warned = true;
            }
            if (other_vertex.has_value() && other_vertex->get().mesh_index != current_vertex.mesh_index) {
                // duplicated
                mapping.add_bidirectional(current_vertex, mapping.map(other_vertex.value()));
            } else {
                grid.insert(position, current_vertex);
                mapping.add_bidirectional(current_vertex, unique_vertices);

                unique_vertices += 1;
            }
        }
    }

    LOG_DEBUG("Identified {} shared and {} unique vertices", maximal_merged_mesh_size - unique_vertices, unique_vertices);
    mapping.validate();

    return mapping;
}

SimpleMesh merge::apply_mapping(std::span<const SimpleMesh> meshes, const merge::VertexMapping &mapping) {
    LOG_TRACE("Merging meshes based on mapping");
    if (meshes.empty()) {
        return {};
    }

    SimpleMesh merged_mesh;

    size_t max_combined_vertex_count = 0;
    size_t max_combined_face_count = 0;
    for (const SimpleMesh &mesh : meshes) {
        max_combined_vertex_count += mesh.vertex_count();
        max_combined_face_count += mesh.face_count();
    }

    const bool has_uvs = std::all_of(meshes.begin(), meshes.end(), [](const SimpleMesh &mesh) {
        return mesh.has_uvs();
    });

    size_t max_vertex_index = 0;
    merged_mesh.positions.resize(max_combined_vertex_count);
    if (has_uvs) {
        merged_mesh.uvs.resize(max_combined_vertex_count);
    }
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
            const size_t mapped_index = mapping.map(VertexId{.mesh_index = mesh_index, .vertex_index = vertex_index});
            merged_mesh.positions[mapped_index] = mesh.positions[vertex_index];
            if (has_uvs) {
                merged_mesh.uvs[mapped_index] = mesh.uvs[vertex_index];
            }
            max_vertex_index = std::max(max_vertex_index, mapped_index);
        }
    }
    assert(max_vertex_index < max_combined_vertex_count || max_vertex_index == 0);
    merged_mesh.positions.resize(max_vertex_index + 1);
    if (has_uvs) {
        merged_mesh.uvs.resize(max_vertex_index + 1);
    }

    merged_mesh.triangles.reserve(max_combined_face_count);
    for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
        const SimpleMesh &mesh = meshes[mesh_index];
        for (size_t triangle_index = 0; triangle_index < mesh.face_count(); triangle_index++) {
            const glm::uvec3 &triangle = mesh.triangles[triangle_index];

            glm::uvec3 new_triangle;
            for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
                new_triangle[k] = mapping.map(VertexId{.mesh_index = mesh_index, .vertex_index = triangle[k]});
            }
            if (new_triangle[0] == new_triangle[1] ||
                new_triangle[1] == new_triangle[2] ||
                new_triangle[2] == new_triangle[0]) {
                LOG_WARN("Skipping illegal triangle while merging");
                continue;
            }

            merged_mesh.triangles.push_back(new_triangle);
        }
    }

    if (!are_all_meshes_merged(mapping)) {
        LOG_WARN("Not all meshes were merged together");
    }

    validate_mesh(merged_mesh);
    validate_mesh(convert::mesh2cgal(merged_mesh));

    return merged_mesh;
}

SimpleMesh merge::merge_meshes(std::span<const SimpleMesh> meshes) {
    VertexMapping mapping;
    return merge::merge_meshes(meshes, mapping);
}

SimpleMesh merge::merge_meshes(std::span<const SimpleMesh> meshes, merge::VertexMapping &mapping) {
    switch (meshes.size()) {
    case 0:
        return {};
    case 1:
        mapping = VertexMapping::identity(meshes[0].vertex_count());
        return meshes[0];
    default:
        mapping = merge::create_merge_mapping(meshes);
        return merge::apply_mapping(meshes, mapping);
    }
}

SimpleMesh merge::merge_meshes(std::span<const SimpleMesh> meshes, double distance_epsilon) {
    VertexMapping mapping;
    return merge::merge_meshes(meshes, distance_epsilon, mapping);
}

SimpleMesh merge::merge_meshes(std::span<const SimpleMesh> meshes, double distance_epsilon, merge::VertexMapping &mapping) {
    switch (meshes.size()) {
    case 0:
        return {};
    case 1:
        mapping = VertexMapping::identity(meshes[0].vertex_count());
        return meshes[0];
    default:
        mapping = merge::create_merge_mapping(meshes, distance_epsilon);
        return merge::apply_mapping(meshes, mapping);
    }
}
