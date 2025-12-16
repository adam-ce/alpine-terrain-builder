#include <random>
#include <ranges>

#include <glm/gtc/type_ptr.hpp>

#include "log.h"
#include "mesh/SimpleMesh.h"
#include "mesh/TriangleSoup.h"
#include "mesh/utils.h"
#include "mesh/validate.h"

std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size) {
    const auto &triangles = mesh.triangles;
    const auto &positions = mesh.positions;
    const size_t num_triangles = triangles.size();

    if (num_triangles == 0) {
        return std::nullopt;
    }

    const size_t triangle_sample_size = std::min((sample_size+2)/3, mesh.face_count());
    const size_t stride = std::max<size_t>(1, num_triangles / triangle_sample_size);

    double total_length = 0.0;

    // Use a small offset to avoid sampling only the first part of the mesh
    const size_t offset = (num_triangles / 7) % num_triangles;

    for (size_t i = 0; i < triangle_sample_size; i++) {
        const auto &tri = triangles[(offset + i * stride) % num_triangles];

        const glm::dvec3 &a = positions[tri.x];
        const glm::dvec3 &b = positions[tri.y];
        const glm::dvec3 &c = positions[tri.z];

        total_length += glm::distance(a, b) + glm::distance(b, c) + glm::distance(c, a);
    }

    return total_length / triangle_sample_size;
}

std::optional<double> calculate_max_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double max_length = 0.0;
    for (const auto &tri : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[tri.x];
        const glm::dvec3 &b = mesh.positions[tri.y];
        const glm::dvec3 &c = mesh.positions[tri.z];

        const double ab = glm::distance2(a, b);
        const double bc = glm::distance2(b, c);
        const double ca = glm::distance2(c, a);

        max_length = std::max({ab, bc, ca, max_length});
    }
    return max_length;
}

std::optional<double> calculate_min_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double max_length = 0.0;
    for (const auto &tri : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[tri.x];
        const glm::dvec3 &b = mesh.positions[tri.y];
        const glm::dvec3 &c = mesh.positions[tri.z];

        const double ab = glm::distance2(a, b);
        const double bc = glm::distance2(b, c);
        const double ca = glm::distance2(c, a);

        max_length = std::min({ab, bc, ca, max_length});
    }
    return max_length;
}

std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh) {
    auto length_sq_opt = calculate_max_edge_length_squared(mesh);
    if (length_sq_opt.has_value()) {
        return std::sqrt(length_sq_opt.value());
    } else {
        return std::nullopt;
    }
}

std::optional<double> calculate_min_edge_length(const SimpleMesh &mesh) {
    auto length_sq_opt = calculate_min_edge_length_squared(mesh);
    if (length_sq_opt.has_value()) {
        return std::sqrt(length_sq_opt.value());
    } else {
        return std::nullopt;
    }
}

size_t remove_isolated_vertices(SimpleMesh &mesh) {
    const bool has_uvs = mesh.has_uvs();
    const std::vector<size_t> isolated = find_isolated_vertices(mesh);

    std::vector<size_t> index_offset;
    for (size_t i : isolated | std::views::reverse) {
        const size_t last_index = mesh.positions.size() - 1;
        std::swap(mesh.positions[i], mesh.positions[last_index]);
        mesh.positions.pop_back();
        if (has_uvs) {
            std::swap(mesh.uvs[i], mesh.uvs[last_index]);
        }
        mesh.uvs.pop_back();

        for (glm::uvec3 &triangle : mesh.triangles) {
            for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
                if (triangle[k] == last_index) {
                    triangle[k] = i;
                }
            }
        }
    }

    return isolated.size();
}

size_t remove_triangles_of_negligible_size(SimpleMesh &mesh, const double threshold_percentage_of_average) {
    std::vector<double> areas;
    areas.reserve(mesh.triangles.size());
    for (glm::uvec3 &triangle : mesh.triangles) {
        const std::array<glm::dvec3, triangle.length()> points{
            mesh.positions[triangle.x],
            mesh.positions[triangle.y],
            mesh.positions[triangle.z]};

        // const double area = Kernel().compute_area_3_object()(cgal_points[0],
        // cgal_points[1], cgal_points[2]);
        const double area =
            0.5 * std::abs(points[0].x * (points[1].y - points[2].y) +
                           points[1].x * (points[2].y - points[0].y) +
                           points[2].x * (points[0].y - points[1].y));

        areas.push_back(area);
    }

    const double average_area =
        std::reduce(areas.begin(), areas.end()) / static_cast<double>(areas.size());
    const size_t erased_count =
        std::erase_if(mesh.triangles, [&](const glm::uvec3 &triangle) {
            const size_t index = &triangle - &*mesh.triangles.begin();
            const double area = areas[index];
            return area < average_area * threshold_percentage_of_average;
        });

    return erased_count;
}

namespace {
template <size_t N>
void normalize_face_index_rotation_impl(std::span<uint32_t, N> face, bool keep_orientation) {
    if (keep_orientation) {
        if (face.empty()) {
            return;
        }

        // find index of minimum element
        size_t min_index = 0;
        for (size_t k = 1; k < face.size(); k++) {
            if (face[k] < face[min_index]) {
                min_index = k;
            }
        }

        // rotate so minimum is first
        if (min_index != 0) {
            std::rotate(face.begin(), face.begin() + min_index, face.end());
        }
    } else {
        std::sort(face.begin(), face.end());
    }
}
}

void normalize_face_index_rotation(const std::span<uint32_t> face, bool keep_orientation) {
    normalize_face_index_rotation_impl<std::dynamic_extent>(face, keep_orientation);
}

void normalize_edge_inplace(glm::uvec2 &edge) {
    if (edge.x > edge.y) {
        std::swap(edge.x, edge.y);
    }
}
glm::uvec2 normalize_edge(glm::uvec2 edge) {
    normalize_edge_inplace(edge);
    return edge;
}

void normalize_triangle_inplace(glm::uvec3 &triangle, bool keep_orientation) {
    std::span<uint32_t, 3> data(glm::value_ptr(triangle), triangle.length());
    normalize_face_index_rotation_impl<3>(data, keep_orientation);
}
glm::uvec3 normalize_triangle(glm::uvec3 triangle, bool keep_orientation) {
    normalize_triangle_inplace(triangle, keep_orientation);
    return triangle;
}

void normalize_quad_inplace(glm::uvec4 &quad, bool keep_orientation) {
    std::span<uint32_t, 4> data(glm::value_ptr(quad), quad.length());
    normalize_face_index_rotation_impl<4>(data, keep_orientation);
}
glm::uvec4 normalize_quad(glm::uvec4 quad, bool keep_orientation) {
    normalize_quad_inplace(quad, keep_orientation);
    return quad;
}

void sort_and_normalize_triangles(std::span<glm::uvec3> triangles) {
    // sort vertices in triangles
    for (glm::uvec3 &triangle : triangles) {
        triangle = normalize_triangle(triangle);
    }

    // sort triangle vector
    std::sort(triangles.begin(), triangles.end(), compare_triangles);
}

template <typename T>
void erase_by_index(std::vector<T> &vec, std::size_t pos) {
    typename std::vector<T>::iterator it = vec.begin();
    std::advance(it, pos);
    vec.erase(it);
}

bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    // First, compare by x
    if (t1.x != t2.x) {
        return t1.x < t2.x;
    }

    // If x is equal, compare by y
    if (t1.y != t2.y) {
        return t1.y < t2.y;
    }

    // If x and y are equal, compare by z
    return t1.z < t2.z;
}
bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    glm::uvec3 t1s(t1);
    glm::uvec3 t2s(t2);

    std::sort(&t1s.x, &t1s.z + 1);
    std::sort(&t2s.x, &t2s.z + 1);

    return compare_triangles(t1s, t2s);
}

bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return normalize_triangle(t1) == normalize_triangle(t2);
}
bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1,
                                                   const glm::uvec3 &t2) {
    return std::is_permutation(&t1.x, &t1.z + 1, &t2.x);
}

namespace {
template <typename IndexContainer>
std::unordered_map<glm::uvec2, IndexContainer>
create_edge_to_triangle_index_mapping_impl(const std::span<const glm::uvec3> triangles) {
    std::unordered_map<glm::uvec2, IndexContainer> edges_to_triangles;

    for_each_edge(triangles, [&](const glm::uvec2 &edge, const size_t triangle_index) {
        auto result = edges_to_triangles.try_emplace(edge, IndexContainer{}).first;
        result->second.push_back(triangle_index);
    }, true);

    return edges_to_triangles;
}
}

std::unordered_map<glm::uvec2, FixedVector<size_t, 2>> create_edge_to_triangle_index_mapping(const SimpleMesh &mesh) {
    return create_edge_to_triangle_index_mapping(mesh.triangles);
}
std::unordered_map<glm::uvec2, FixedVector<size_t, 2>> create_edge_to_triangle_index_mapping(const std::span<const glm::uvec3> triangles) {
    return detail::create_edge_to_triangle_index_mapping_impl<FixedVector<size_t, 2>>(triangles);
}

std::unordered_map<glm::uvec2, HybridVector<size_t, 2>> create_edge_to_triangle_index_mapping_non_manifold(const SimpleMesh &mesh) {
    return create_edge_to_triangle_index_mapping_non_manifold(mesh.triangles);
}
std::unordered_map<glm::uvec2, sHybridVector<size_t, 2>> create_edge_to_triangle_index_mapping_non_manifold(const std::span<const glm::uvec3> triangles) {
    return detail::create_edge_to_triangle_index_mapping_impl<HybridVector<size_t, 2>>(triangles);
}


namespace {
struct TriangleHash {
    size_t operator()(const glm::uvec3 &t) const {
        return std::hash<uint32_t>()(t.x) ^ std::hash<uint32_t>()(t.y) ^ std::hash<uint32_t>()(t.z);
    }
};

struct TriangleEquals {
    bool operator()(const glm::uvec3 &a, const glm::uvec3 &b) const noexcept {
        return normalize_triangle(a) == normalize_triangle(b);
    }
};
}

std::vector<size_t> find_duplicate_triangles_consider_orientation(const std::span<const glm::uvec3> triangles) {
    std::vector<size_t> triangles_to_remove;
    std::unordered_set<glm::uvec3, TriangleHash, TriangleEquals> unique_triangles;

    for (size_t i = 0; i < triangles.size(); i++) {
        const glm::uvec3& triangle = triangles[i];
        if (unique_triangles.find(triangle) != unique_triangles.end()) {
            triangles_to_remove.push_back(i);
        } else {
            unique_triangles.insert(triangle);
        }
    }

    return triangles_to_remove;
}

void remove_duplicate_triangles_consider_orientation(std::vector<glm::uvec3> &triangles) {
    remove_duplicate_triangles<double>(triangles, {}, false);
}

std::vector<size_t> count_vertex_adjacent_triangles(const SimpleMesh &mesh) {
    std::vector<size_t> adjacent_triangle_count(mesh.vertex_count(), 0);

    for (const glm::uvec3 &triangle : mesh.triangles) {
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            adjacent_triangle_count[triangle[k]]++;
        }
    }

    return adjacent_triangle_count;
}

std::vector<glm::uvec2> find_non_manifold_edges(const SimpleMesh &mesh) {
    std::unordered_map<glm::uvec2, std::vector<size_t>> edges_to_triangles = create_edge_to_triangle_index_mapping_non_manifold(mesh);
    std::vector<glm::uvec2> non_manifold_edges;

    for (const auto& entry : edges_to_triangles) {
        const glm::uvec2 edge = entry.first;
        const std::vector<size_t> &triangle_indices = entry.second;

        if (triangle_indices.size() > 2) {
            non_manifold_edges.push_back(edge);
        }
    }

    return non_manifold_edges;
}

std::vector<size_t> find_single_non_manifold_triangle_indices(const SimpleMesh &mesh) {
    const std::vector<size_t> adjacent_triangle_count =
        count_vertex_adjacent_triangles(mesh);
    const std::unordered_map<glm::uvec2, std::vector<size_t>> edges_to_triangles =
        create_edge_to_triangle_index_mapping_non_manifold(mesh);

    std::vector<size_t> non_manifold_triangles;
    for (auto entry : edges_to_triangles) {
        const glm::uvec2 edge = entry.first;
        const std::vector<size_t> &triangle_indices = entry.second;

        if (triangle_indices.size() <= 2) {
            continue;
        }

        for (const size_t triangle_index : triangle_indices) {
            const glm::uvec3 triangle = mesh.triangles[triangle_index];
            for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
                if (triangle[k] == edge[0] || triangle[k] == edge[1]) {
                    continue;
                }

                // We check if the third vertex of the triangle with the non-manifold
                // edge is unconnected as we can be sure in this case that its a flap.
                // TODO: a general flap detection method would need to change this part.
                if (adjacent_triangle_count[triangle[k]] <= 1) {
                    non_manifold_triangles.push_back(triangle_index);
                    break;
                }
            }
        }
    }

    return non_manifold_triangles;
}

void remove_single_non_manifold_triangles(SimpleMesh & mesh) {
    std::vector<size_t> non_manifold_triangles = find_single_non_manifold_triangle_indices(mesh);

    std::sort(non_manifold_triangles.begin(),
                non_manifold_triangles.end(),
                std::greater<size_t>());

    for (const size_t triangle_index : non_manifold_triangles) {
        erase_by_index(mesh.triangles, triangle_index);
    }

    remove_isolated_vertices(mesh);
}

void reindex_mesh(SimpleMesh & mesh) {
    struct Entry {
        uint32_t new_index;
        uint32_t inv_index;
    };

    const uint32_t invalid_index = static_cast<uint32_t>(-1);
    const Entry invalid_entry = Entry{invalid_index, invalid_index};
    std::vector<Entry> index_map(mesh.positions.size(), invalid_entry);

    // Adjust triangles
    uint32_t next_new_index = 0;
    for (auto &triangle : mesh.triangles) {
        for (uint32_t i = 0; i < 3; i++) {
            Entry &entry = index_map[triangle[i]];
            if (entry.new_index == invalid_index) {
                // Vertex newly encountered
                entry.new_index = triangle[i] = next_new_index;
                next_new_index += 1;
            } else {
                // Vertex already encountered
                triangle[i] = entry.new_index;
            }
        }
    }
    const uint32_t new_vertex_count = next_new_index;

    // Add the inverse index
    for (uint32_t old_index = 0; old_index < index_map.size(); old_index++) {
        Entry &entry = index_map[old_index];
        if (entry.new_index == invalid_index) {
            // This vertex was not used in any triangle
            continue;
        }
        index_map[entry.new_index].inv_index = old_index;
    }

    // Adjust vertices
    for (uint32_t old_index = 0; old_index < new_vertex_count; old_index++) {
        const Entry entry = index_map[old_index];
        std::swap(mesh.positions[old_index], mesh.positions[entry.inv_index]);
        if (mesh.has_uvs()) {
            std::swap(mesh.uvs[old_index], mesh.uvs[entry.inv_index]);
        }
        index_map[entry.inv_index].new_index = entry.new_index;

        if (entry.new_index != invalid_index) {
            index_map[entry.new_index].inv_index = entry.inv_index;
        }
    }

    // Remove unused vertices
    mesh.positions.resize(new_vertex_count);
    if (mesh.has_uvs()) {
        mesh.uvs.resize(new_vertex_count);
    }
}

SimpleMesh reindex_mesh(const SimpleMesh &mesh) {
    std::vector<glm::uvec3> new_triangles;
    new_triangles.reserve(mesh.face_count());
    std::vector<glm::dvec3> new_positions;
    new_positions.reserve(mesh.vertex_count());
    std::vector<glm::dvec2> new_uvs;
    if (mesh.has_uvs()) {
        new_uvs.reserve(mesh.vertex_count());
    }

    const uint32_t invalid_index = static_cast<uint32_t>(-1);
    std::vector<uint32_t> index_map(mesh.positions.size(), invalid_index);

    for (const auto &triangle : mesh.triangles) {
        glm::uvec3 new_triangle_indices;
        for (size_t i = 0; i < 3; i++) {
            const uint32_t old_index = triangle[i];
            if (index_map[old_index] == invalid_index) {
                // Vertex newly encountered
                const uint32_t new_index = new_positions.size();
                new_positions.push_back(mesh.positions[old_index]);
                if (mesh.has_uvs()) {
                    new_uvs.push_back(mesh.uvs[old_index]);
                }
                new_triangle_indices[i] = new_index;
                index_map[old_index] = new_index;
            } else {
                // Vertex already encountered
                new_triangle_indices[i] = index_map[old_index];
            }
        }
        new_triangles.push_back(new_triangle_indices);
    }

    SimpleMesh new_mesh(new_triangles, new_positions);
    if (mesh.has_uvs()) {
        new_mesh.uvs = std::move(new_uvs);
    }
    new_mesh.texture = mesh.texture;
    return new_mesh;
}

void flip_triangle_orientation(glm::uvec3 & triangle) {
    std::swap(triangle.z, triangle.x);
}
void flip_triangle_orientations(std::vector<glm::uvec3> & triangles) {
    for (auto &triangle : triangles) {
        flip_triangle_orientation(triangle);
    }
}

bool is_degenerate(const glm::uvec3 &triangle) {
    return triangle[0] == triangle[1] || triangle[1] == triangle[2] || triangle[2] == triangle[0];
}
