#include <random>
#include <ranges>

#include "SimpleMesh.h"
#include "utils.h"

radix::geometry::Aabb3d calculate_bounds(const SimpleMesh &mesh) {
    radix::geometry::Aabb3d bounds;
    bounds.min = glm::dvec3(std::numeric_limits<double>::infinity());
    bounds.max = glm::dvec3(-std::numeric_limits<double>::infinity());
    for (unsigned int j = 0; j < mesh.positions.size(); j++) {
        const auto &position = mesh.positions[j];
        bounds.expand_by(position);
    }
    return bounds;
}

radix::geometry::Aabb3d calculate_bounds(std::span<const SimpleMesh> meshes) {
    radix::geometry::Aabb3d bounds;
    bounds.min = glm::dvec3(std::numeric_limits<double>::infinity());
    bounds.max = glm::dvec3(-std::numeric_limits<double>::infinity());
    for (unsigned int i = 0; i < meshes.size(); i++) {
        const SimpleMesh &mesh = meshes[i];
        for (unsigned int j = 0; j < mesh.positions.size(); j++) {
            const auto &position = mesh.positions[j];
            bounds.expand_by(position);
        }
    }
    return bounds;
}

std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size) {
    const auto &triangles = mesh.triangles;
    if (triangles.empty()) {
        return std::nullopt;
    }

    // Random sampling setup
    std::vector<glm::uvec3> sampled_triangles;
    sampled_triangles.reserve(std::min(sample_size, triangles.size()));

    std::random_device rd;
    std::mt19937 rng(rd());
    std::sample(triangles.begin(), triangles.end(),
                std::back_inserter(sampled_triangles),
                std::min(sample_size, triangles.size()),
                rng);

    double total_length = 0.0;
    size_t edge_count = 0;

    for (const auto &tri : sampled_triangles) {
        const glm::dvec3 &a = mesh.positions[tri.x];
        const glm::dvec3 &b = mesh.positions[tri.y];
        const glm::dvec3 &c = mesh.positions[tri.z];

        total_length += glm::distance(a, b);
        total_length += glm::distance(b, c);
        total_length += glm::distance(c, a);

        edge_count += 3;
    }

    assert(edge_count > 0);
    return total_length / edge_count;
}

std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double max_length = 0.0;
    for (const auto &tri : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[tri.x];
        const glm::dvec3 &b = mesh.positions[tri.y];
        const glm::dvec3 &c = mesh.positions[tri.z];

        const double ab = glm::distance(a, b);
        const double bc = glm::distance(b, c);
        const double ca = glm::distance(c, a);

        max_length = std::max({ab, bc, ca, max_length});
    }
    return max_length;
}

std::vector<size_t> find_isolated_vertices(const SimpleMesh& mesh) {
    std::vector<bool> connected;
    connected.resize(mesh.vertex_count());
    std::fill(connected.begin(), connected.end(), false);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            connected[triangle[k]] = true;
        }
    }

    std::vector<size_t> isolated;
    for (size_t i = 0; i < mesh.vertex_count(); i++) {
        if (!connected[i]) {
            isolated.push_back(i);
        }
    }

    return isolated;
}

size_t remove_isolated_vertices(SimpleMesh& mesh) {
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

size_t remove_triangles_of_negligible_size(SimpleMesh& mesh, const double threshold_percentage_of_average) {
    std::vector<double> areas;
    areas.reserve(mesh.triangles.size());
    for (glm::uvec3 &triangle : mesh.triangles) {
        const std::array<glm::dvec3, triangle.length()> points{
            mesh.positions[triangle.x],
            mesh.positions[triangle.y],
            mesh.positions[triangle.z]};

        // const double area = Kernel().compute_area_3_object()(cgal_points[0], cgal_points[1], cgal_points[2]);
        const double area = 0.5 * std::abs(
                                      points[0].x * (points[1].y - points[2].y) +
                                      points[1].x * (points[2].y - points[0].y) +
                                      points[2].x * (points[0].y - points[1].y));

        areas.push_back(area);
    }

    const double average_area = std::reduce(areas.begin(), areas.end()) / static_cast<double>(areas.size());
    const size_t erased_count = std::erase_if(mesh.triangles, [&](const glm::uvec3 &triangle) {
        const size_t index = &triangle - &*mesh.triangles.begin();
        const double area = areas[index];
        return area < average_area * threshold_percentage_of_average;
    });

    return erased_count;
}

static glm::uvec3 normalize_triangle(const glm::uvec3 &triangle) {
    unsigned int min_index = 0;
    for (size_t k = 1; k < static_cast<size_t>(triangle.length()); k++) {
        if (triangle[min_index] > triangle[k]) {
            min_index = k;
        }
    }
    if (min_index == 0) {
        return triangle;
    }

    glm::uvec3 normalized_triangle;
    for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
        normalized_triangle[k] = triangle[(min_index + k) % triangle.length()];
    }

    return normalized_triangle;
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
bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return std::is_permutation(&t1.x, &t1.z + 1, &t2.x);
}

void remove_duplicate_triangles(SimpleMesh &mesh, bool ignore_orientation) {
    remove_duplicate_triangles(mesh.triangles, ignore_orientation);
}
void remove_duplicate_triangles(std::vector<glm::uvec3> &triangles, bool ignore_orientation) {
    triangles.erase(find_duplicate_triangles(triangles, ignore_orientation), triangles.end());
}

std::unordered_map<glm::uvec2, std::vector<size_t>> create_edge_to_triangle_index_mapping(const SimpleMesh &mesh) {
    std::unordered_map<glm::uvec2, std::vector<size_t>> edges_to_triangles;
    for (size_t i = 0; i < mesh.face_count(); i++) {
        glm::uvec3 triangle = mesh.triangles[i];
        std::sort(&triangle.x, &triangle.z + 1);

        const std::array<glm::uvec2, 3> edges{
            glm::uvec2(triangle.x, triangle.y),
            glm::uvec2(triangle.y, triangle.z),
            glm::uvec2(triangle.x, triangle.z)};

        for (const glm::uvec2 edge : edges) {
            auto result = edges_to_triangles.try_emplace(edge, std::vector<size_t>()).first;
            std::vector<size_t> &list = result->second;
            list.push_back(i);
        }
    }
    return edges_to_triangles;
}

std::vector<size_t> count_vertex_adjacent_triangles(const SimpleMesh& mesh) {
    std::vector<size_t> adjacent_triangle_count(mesh.vertex_count(), 0);

    for (const glm::uvec3& triangle : mesh.triangles) {
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            adjacent_triangle_count[triangle[k]]++;
        }
    }

    return adjacent_triangle_count;
}

std::vector<glm::uvec2> find_non_manifold_edges(const SimpleMesh& mesh) {
    std::unordered_map<glm::uvec2, std::vector<size_t>> edges_to_triangles = create_edge_to_triangle_index_mapping(mesh);
    std::vector<glm::uvec2> non_manifold_edges;

    for (auto entry : edges_to_triangles) {
        const glm::uvec2 edge = entry.first;
        const std::vector<size_t> &triangle_indices = entry.second;

        if (triangle_indices.size() > 2) {
            non_manifold_edges.push_back(edge);
        }
    }

    return non_manifold_edges;
}

std::vector<size_t> find_single_non_manifold_triangle_indices(const SimpleMesh &mesh) {
    const std::vector<size_t> adjacent_triangle_count = count_vertex_adjacent_triangles(mesh);
    const std::unordered_map<glm::uvec2, std::vector<size_t>> edges_to_triangles = create_edge_to_triangle_index_mapping(mesh);

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

                // We check if the third vertex of the triangle with the non-manifold edge is unconnected
                // as we can be sure in this case that its a flap.
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

void remove_single_non_manifold_triangles(SimpleMesh& mesh) {
    std::vector<size_t> non_manifold_triangles = find_single_non_manifold_triangle_indices(mesh);

    std::sort(non_manifold_triangles.begin(), non_manifold_triangles.end(), std::greater<size_t>());

    for (const size_t triangle_index : non_manifold_triangles) {
        erase_by_index(mesh.triangles, triangle_index);
    }

    remove_isolated_vertices(mesh);
}

void sort_and_normalize_triangles(SimpleMesh& mesh) {
    sort_and_normalize_triangles(mesh.triangles);
}
void sort_and_normalize_triangles(std::span<glm::uvec3> triangles) {
    // sort vertices in triangles
    for (glm::uvec3 &triangle : triangles) {
        triangle = normalize_triangle(triangle);
    }

    // sort triangle vector
    std::sort(triangles.begin(), triangles.end(), compare_triangles);
}

static void validate_sorted_normalized_mesh(const SimpleMesh &mesh) {
    // check correct count of uvs
    assert(!mesh.has_uvs() || mesh.positions.size() == mesh.uvs.size());

    // check uvs between 0 and 1
    for (const glm::dvec2 &uv : mesh.uvs) {
        for (size_t k = 0; k < static_cast<size_t>(uv.length()); k++) {
            assert(uv[k] >= 0);
            assert(uv[k] <= 1);
        }
    }

    // check for vertex indices in triangles outside valid range
    for (const glm::uvec3 &triangle : mesh.triangles) {
        for (size_t k = 0; k < static_cast<size_t>(triangle.length()); k++) {
            const size_t vertex_index = triangle[k];
            assert(vertex_index < mesh.vertex_count());
        }
    }

    // check for degenerate triangles
    for (const glm::uvec3 &triangle : mesh.triangles) {
        assert(triangle.x != triangle.y);
        assert(triangle.y != triangle.z);
    }

    // check for duplicated triangles
    assert(mesh.triangles.end() == std::adjacent_find(mesh.triangles.begin(), mesh.triangles.end()));

    // check for duplicated triangles with different orientation
    std::vector<glm::uvec3> triangles_ignore_orientation(mesh.triangles);
    // sort vertices in triangles
    for (glm::uvec3 &triangle : triangles_ignore_orientation) {
        std::sort(&triangle.x, &triangle.z);
    }
    std::vector<glm::uvec3> triangles_ignore_orientation2(triangles_ignore_orientation);
    sort_and_normalize_triangles(triangles_ignore_orientation);
    assert(triangles_ignore_orientation.end() == std::adjacent_find(triangles_ignore_orientation.begin(), triangles_ignore_orientation.end()));

    // check for isolated vertices
    assert(find_isolated_vertices(mesh).empty());
}

void validate_mesh(const SimpleMesh &mesh) {
#if NDEBUG
    return;
#endif
    SimpleMesh sorted(mesh);
    sort_and_normalize_triangles(sorted);
    validate_sorted_normalized_mesh(sorted);
}
