// required before distance.h due to a bug in cgal
// https://github.com/CGAL/cgal/issues/8009
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel_with_sqrt.h>

#include <CGAL/Polygon_mesh_processing/distance.h>
#include <CGAL/Polygon_mesh_processing/repair_self_intersections.h>
#include <CGAL/Surface_mesh_simplification/Edge_collapse_visitor_base.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Bounded_normal_change_filter.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Constrained_placement.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/GarlandHeckbert_plane_policies.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/LindstromTurk_cost.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/LindstromTurk_placement.h>
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>
#include <CGAL/tags.h>

#include <fmt/core.h>

#include "mesh/convert.h"
#include "mesh/bounds.h"
#include "opencv_utils.h"
#include "log.h"
#include "simplify.h"
#include "uv_map.h"
#include "mesh/cleanup.h"
#include "mesh/validate.h"
#include "mesh/cgal.h"


auto fmt::formatter<simplify::Algorithm>::format(const simplify::Algorithm &algorithm, format_context &ctx) const {
    string_view name = "unknown";

    switch (algorithm) {
    case simplify::Algorithm::GarlandHeckbert:
        name = "GarlandHeckbert";
        break;
    case simplify::Algorithm::LindstromTurk:
        name = "LindstromTurk";
        break;
    }

    return formatter<string_view>::format(name, ctx);
}

std::ostream &operator<<(std::ostream &os, const simplify::Algorithm &algorithm) {
    os << fmt::format("{}", algorithm);
    return os;
}

namespace simplify {

namespace {
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

// We use a different uv map type here, because we need this one to be attached to the mesh
// as otherwise the entries for removed vertices are not removed during garbage collection.
// We could use the same type as in uv_map but this would require a custom visitor or similar.
using AttachedUvPropertyMap = cgal::Mesh::Property_map<cgal::VertexDescriptor, cgal::Point2>;

inline std::vector<glm::dvec2> decode_uv_map(const AttachedUvPropertyMap &uv_map, size_t number_of_vertices) {
    std::vector<glm::dvec2> uvs;
    uvs.reserve(number_of_vertices);
    for (size_t i = 0; i < number_of_vertices; i++) {
        const cgal::Point2 &uv = uv_map[CGAL::SM_Vertex_index(i)];
        uvs.push_back(convert::to_glm_point(uv));
    }
    return uvs;
}

template <class T>
T clone(const T &orig) {
    return T{orig};
}

class ExpensiveStopPredicate {
public:
    ExpensiveStopPredicate(cgal::Mesh& mesh) : mesh(mesh), original_mesh(clone(mesh)) {
        if (this->has_expensive_checks()) {
            this->make_snapshot(std::move(clone(mesh)), mesh);
        }
    }

    template <typename F, typename Profile>
    bool operator()(const F & /*current_cost*/,
                    const Profile &/*profile*/,
                    size_t /*initial_edge_count*/,
                    size_t /*current_edge_count*/) const {
        if (this->has_stopped) {
            return true;
        }

        if (this->has_expensive_checks() && this->should_check_expensive(this->original_mesh, this->mesh)) {
            // At this point we know that we will perform expensive checks
            // and thus want to clean up the removed geometry.
            cgal::Mesh mesh_clone = clone(this->mesh);
            mesh_clone.collect_garbage();

            if (this->should_stop(this->original_mesh, mesh_clone, true)) {
                this->restore_snapshot(this->mesh);
                this->has_stopped = true;
                return true;
            } else {
                this->make_snapshot(std::move(mesh_clone), this->mesh);
                return false;
            }
        }

        if (this->should_stop(this->original_mesh, this->mesh, false)) {
            this->has_stopped = true;
            return true;
        } else {
            return false;
        }
    }

protected:
    virtual bool has_expensive_checks() const {
        return true;
    };

    virtual bool should_check_expensive(const cgal::Mesh &original, const cgal::Mesh &simplified) const = 0;
    virtual bool should_stop(const cgal::Mesh &original, const cgal::Mesh &simplified, const bool check_expensive) const = 0;

    virtual void make_snapshot(cgal::Mesh &&/*mesh_copy*/, cgal::Mesh& mesh) const {
        this->mesh_snapshot = mesh;
    }
    virtual void restore_snapshot(cgal::Mesh& mesh) const {
        mesh = this->mesh_snapshot;
    }

private:
    mutable bool has_stopped = false;
    cgal::Mesh &mesh;
    mutable cgal::Mesh mesh_snapshot;
    mutable cgal::Mesh original_mesh;
};

inline double measure_max_absolute_error(const cgal::Mesh &original, const cgal::Mesh &simplified, const double bound_on_error = 0.0001) {
    const double error = CGAL::Polygon_mesh_processing::bounded_error_Hausdorff_distance<CGAL::Parallel_if_available_tag, cgal::Mesh, cgal::Mesh>(
        original, simplified, bound_on_error);
    return error + bound_on_error;
}

inline radix::geometry::Aabb3d calculate_bounds(const cgal::Mesh &mesh) {
    radix::geometry::Aabb3d bounds;
    bounds.min = glm::dvec3(std::numeric_limits<double>::infinity());
    bounds.max = glm::dvec3(-std::numeric_limits<double>::infinity());

    for (const CGAL::SM_Vertex_index vertex_index : mesh.vertices()) {
        const cgal::Point3 &position = mesh.point(vertex_index);
        bounds.expand_by(convert::to_glm_point(position));
    }
    return bounds;
}

inline std::pair<bool, double> check_condition(const VertexRatio &vertex_ratio, const cgal::Mesh &modified, const cgal::Mesh &original) {
    const double modified_vertex_count = modified.number_of_vertices();
    const double current_ratio = static_cast<double>(modified_vertex_count) / original.num_vertices();
    // LOG_TRACE("Current vertex ratio is {:g}% with target {:g}%", current_ratio * 100, vertex_ratio.ratio * 100);
    DEBUG_ASSERT(current_ratio >= 0 && current_ratio <= 1);
    const bool fulfilled = current_ratio <= vertex_ratio.ratio;
    return {fulfilled, current_ratio};
}
inline std::pair<bool, double> check_condition(const EdgeRatio &edge_ratio, const cgal::Mesh &modified, const cgal::Mesh &original) {
    const double modified_edge_count = modified.number_of_edges();
    const double current_ratio = static_cast<double>(modified_edge_count) / original.num_edges();
    // LOG_TRACE("Current edge ratio is {:g}% with target {:g}%", current_ratio * 100, edge_ratio.ratio * 100);
    DEBUG_ASSERT(current_ratio >= 0 && current_ratio <= 1);
    const bool fulfilled = current_ratio <= edge_ratio.ratio;
    return {fulfilled, current_ratio};
}
inline std::pair<bool, double> check_condition(const FaceRatio &face_ratio, const cgal::Mesh &modified, const cgal::Mesh &original) {
    const double modified_face_count = modified.number_of_faces();
    const double current_ratio = static_cast<double>(modified_face_count) / original.num_faces();
    // LOG_TRACE("Current face ratio is {:g}% with target {:g}%", current_ratio * 100, face_ratio.ratio * 100);
    DEBUG_ASSERT(current_ratio >= 0 && current_ratio <= 1);
    const bool fulfilled = current_ratio <= face_ratio.ratio;
    return {fulfilled, current_ratio};
}
inline std::pair<bool, double> check_condition(const RelativeError &relative_error, const cgal::Mesh &modified, const cgal::Mesh &original) {
    // TODO: use CGAL::Polygon_mesh_processing::is_Hausdorff_distance_larger() instead
    const glm::dvec3 original_mesh_size = calculate_bounds(original).size();
    const double original_mesh_max_size = std::max({original_mesh_size[0], original_mesh_size[1], original_mesh_size[2]});
    const double absolute_error_bound = relative_error.error_bound * original_mesh_max_size;
    const double current_absolute_error = measure_max_absolute_error(original, modified, absolute_error_bound * 0.1 /* TODO: */);
    const double current_relative_error = current_absolute_error / original_mesh_max_size;
    LOG_TRACE("Current relative error is {:g}% with target {:g}%", current_relative_error * 100, relative_error.error_bound * 100);
    const bool fulfilled = current_relative_error >= relative_error.error_bound;
    return {fulfilled, current_relative_error};
}
inline std::pair<bool, double> check_condition(const AbsoluteError absolute_error, const cgal::Mesh &modified, const cgal::Mesh &original) {
    const double current_absolute_error = measure_max_absolute_error(convert::to_cgal_mesh(convert::to_simple_mesh(original)), convert::to_cgal_mesh(convert::to_simple_mesh(modified)), absolute_error.error_bound * 0.1 /* TODO: */);
    LOG_TRACE("Current absolute error is {:g} with target {:g}", current_absolute_error, absolute_error.error_bound);
    const bool fulfilled = current_absolute_error >= absolute_error.error_bound;
    return {fulfilled, current_absolute_error};
}
inline std::pair<bool, double> check_condition(const StopCondition &stop_condition, const cgal::Mesh &modified, const cgal::Mesh &original) {
    return std::visit(overloaded{
                          [&](const VertexRatio &vertex_ratio) {
                              return check_condition(vertex_ratio, modified, original);
                          },
                          [&](const EdgeRatio &edge_ratio) {
                              return check_condition(edge_ratio, modified, original);
                          },
                          [&](const FaceRatio &face_ratio) {
                              return check_condition(face_ratio, modified, original);
                          },
                          [&](const RelativeError &relative_error) {
                              return check_condition(relative_error, modified, original);
                          },
                          [&](const AbsoluteError &absolute_error) {
                              return check_condition(absolute_error, modified, original);
                          }},
                      stop_condition);
}

inline bool is_evaluation_expensive(const StopCondition &stop_condition) {
    return std::visit(overloaded{
                          [&](const VertexRatio &) {
                              return false;
                          },
                          [&](const EdgeRatio &) {
                              return false;
                          },
                          [&](const FaceRatio &) {
                              return false;
                          },
                          [&](const RelativeError &) {
                              return true;
                          },
                          [&](const AbsoluteError &) {
                              return true;
                          }},
                      stop_condition);
}

struct MeshSnapshot {
    // mesh is already saved by ExpensiveStopPredicate
    std::vector<glm::dvec2> uv_map;
};

class StopConditionStopPredicate : public ExpensiveStopPredicate {
public:
    StopConditionStopPredicate(cgal::Mesh &mesh, AttachedUvPropertyMap &uv_map, const std::span<const StopCondition> stop_conditions)
        : ExpensiveStopPredicate(mesh), stop_conditions(stop_conditions), uv_map_ref(uv_map) {
        this->has_expensive_condition = std::any_of(this->stop_conditions.begin(), this->stop_conditions.end(), is_evaluation_expensive);
        this->next_check_edge_count = CGAL::num_edges(mesh) * 0.9;
        this->last_snapshot = {};
    }

protected:
    bool has_expensive_checks() const override {
        return this->has_expensive_condition;
    }
    bool should_check_expensive(const cgal::Mesh &, const cgal::Mesh &simplified) const override {
        if (!this->has_expensive_condition) {
            return false;
        }

        const double current_edge_count = simplified.number_of_edges();
        if (this->next_check_edge_count >= current_edge_count) {
            LOG_TRACE("Current edge count threshold of {} reached", this->next_check_edge_count);
            this->next_check_edge_count *= 0.9;
            LOG_TRACE("Next expensive stop condition check is at edge count of {}", this->next_check_edge_count);
            return true;
        }
        return false;
    }

    bool should_stop(const cgal::Mesh &original, const cgal::Mesh &simplified, const bool check_expensive) const override {
        return std::any_of(this->stop_conditions.begin(), this->stop_conditions.end(), [&](const StopCondition &stop_condition) {
            if (is_evaluation_expensive(stop_condition) && !check_expensive) {
                return false;
            }
            return check_condition(stop_condition, simplified, original).first;
        });
    }

    void make_snapshot(cgal::Mesh &&mesh, cgal::Mesh& mesh2) const override {
        LOG_TRACE("Making new snapshot of current geometry");
        this->last_snapshot.uv_map.clear();
        this->last_snapshot.uv_map.resize(mesh2.num_vertices());

        for (const CGAL::SM_Vertex_index vertex_index : mesh2.vertices()) {
            const cgal::Point2 &uv = uv_map_ref[vertex_index];
            this->last_snapshot.uv_map[vertex_index] = convert::to_glm_point(uv);
        }
        
        ExpensiveStopPredicate::make_snapshot(std::move(mesh), mesh2);
    }

    void restore_snapshot(cgal::Mesh& mesh) const override {
        LOG_TRACE("Restoring last valid snapshot");
        ExpensiveStopPredicate::restore_snapshot(mesh);

        mesh.remove_property_map(this->uv_map_ref);
        this->uv_map_ref = mesh.add_property_map<cgal::VertexDescriptor, cgal::Point2>("h:uv").first;
        for (size_t i = 0; i < this->last_snapshot.uv_map.size(); i++) {
            this->uv_map_ref[CGAL::SM_Vertex_index(i)] = convert::to_cgal_point(this->last_snapshot.uv_map[i]);
        }
    }

private:
    const std::span<const StopCondition> stop_conditions;
    AttachedUvPropertyMap &uv_map_ref;
    mutable size_t next_check_edge_count;
    bool has_expensive_condition;
    mutable MeshSnapshot last_snapshot;
};

struct UvMapUpdateEdgeCollapseVisitor : CGAL::Surface_mesh_simplification::Edge_collapse_visitor_base<cgal::Mesh> {
    UvMapUpdateEdgeCollapseVisitor(AttachedUvPropertyMap &uv_map)
        : uv_map(uv_map) {}

    // Called during the processing phase for each edge being collapsed.
    // If placement is absent the edge is left uncollapsed.
    void OnCollapsing(const Profile &profile, std::optional<Point> placement) {
        if (!placement) {
            return;
        }

        const cgal::VertexDescriptor v0 = profile.v0();
        const cgal::VertexDescriptor v1 = profile.v1();

        const glm::dvec3 pt = convert::to_glm_point(*placement);
        const glm::dvec3 p0 = convert::to_glm_point(profile.p0());
        const glm::dvec3 p1 = convert::to_glm_point(profile.p1());

        const auto w1 = std::clamp(glm::length(pt - p0) / glm::length(p1 - p0), 0.0, 1.0);
        const auto w0 = 1 - w1;

        const glm::dvec2 uv0 = convert::to_glm_point(get(uv_map, v0));
        const glm::dvec2 uv1 = convert::to_glm_point(get(uv_map, v1));
        this->new_uv = convert::to_cgal_point(uv0 * w0 + uv1 * w1);
    }

    // Called after each edge has been collapsed
    void OnCollapsed(const Profile &, cgal::VertexDescriptor vd) {
        uv_map[vd] = new_uv;
    }

    AttachedUvPropertyMap &uv_map;
    cgal::Point2 new_uv;
};

// Property map that indicates whether an edge is marked as non-removable.
struct BorderIsConstrainedEdgeMap {
    typedef cgal::EdgeDescriptor key_type;
    typedef bool value_type;
    typedef value_type reference;
    typedef boost::readable_property_map_tag category;

    const cgal::Mesh *mesh;
    const bool active = true;
    const bool restrict_border_triangles = true;

    BorderIsConstrainedEdgeMap(const cgal::Mesh &mesh, const bool active = true)
        : mesh(&mesh), active(active) {}

    friend value_type get(const BorderIsConstrainedEdgeMap &map, const key_type &edge) {
        if (!map.active) {
            return false;
        }

        const cgal::Mesh &mesh = *map.mesh;

        // return CGAL::is_border(edge, mesh); // old version
        if (CGAL::is_border(edge, mesh)) {
            return true;
        }

        if (!map.restrict_border_triangles) {
            return false;
        }

        const cgal::VertexDescriptor v0 = mesh.vertex(edge, 0);
        const cgal::VertexDescriptor v1 = mesh.vertex(edge, 1);
        if (CGAL::is_border(v0, mesh).has_value() || CGAL::is_border(v1, mesh).has_value()) {
            return true;
        }

        return false;
    }
};

struct SimplificationArgs {
    bool lock_borders;
    std::span<const StopCondition> stop_conditions;
};

template <class Cost, class Placement>
inline size_t _simplify_mesh_with_cost_and_placement(
    cgal::Mesh &mesh,
    AttachedUvPropertyMap &uv_map,
    const Cost &cost,
    const Placement &placement,
    const SimplificationArgs args) {
    typedef typename CGAL::Surface_mesh_simplification::Constrained_placement<Placement, BorderIsConstrainedEdgeMap> ConstrainedPlacement;
    BorderIsConstrainedEdgeMap bem(mesh, args.lock_borders);
    const ConstrainedPlacement constrained_placement(bem, placement);

    // const CGAL::Surface_mesh_simplification::Count_ratio_stop_predicate<cgal::Mesh> stop_predicate(args.stop_edge_ratio);
    const StopConditionStopPredicate stop_predicate(mesh, uv_map, args.stop_conditions);
    UvMapUpdateEdgeCollapseVisitor visitor(uv_map);
    const CGAL::Surface_mesh_simplification::Bounded_normal_change_filter<> filter;

    const size_t removed_edge_count = CGAL::Surface_mesh_simplification::edge_collapse(mesh, stop_predicate,
                                                                                       CGAL::parameters::edge_is_constrained_map(bem)
                                                                                           .get_placement(constrained_placement)
                                                                                           .get_cost(cost)
                                                                                           .filter(filter)
                                                                                           .visitor(visitor));

    LOG_TRACE("Removed {} edges from simplified mesh", removed_edge_count);

    // Actually remove the vertices, edges and faces
    mesh.collect_garbage();

    return removed_edge_count;
}

template <class Policies>
inline size_t _simplify_mesh_with_policies(
    cgal::Mesh &mesh,
    AttachedUvPropertyMap &uv_map,
    const Policies &policies,
    const SimplificationArgs args) {
    typedef typename Policies::Get_cost Cost;
    typedef typename Policies::Get_placement Placement;

    const Cost &cost = policies.get_cost();
    const Placement &placement = policies.get_placement();

    return _simplify_mesh_with_cost_and_placement(mesh, uv_map, cost, placement, args);
}

inline size_t _simplify_mesh(
    cgal::Mesh &mesh,
    AttachedUvPropertyMap &uv_map,
    const Algorithm algorithm,
    const SimplificationArgs args) {
    // LOG_TRACE("Simplifying mesh (stop ratio={:g}, borders={}, algorithm={})", args.stop_edge_ratio, args.lock_borders ? "Locked" : "Unlocked", algorithm);

    switch (algorithm) {
        case Algorithm::GarlandHeckbert:
            typedef CGAL::Surface_mesh_simplification::GarlandHeckbert_plane_policies<cgal::Mesh, cgal::Kernel> GH_policies;
            return _simplify_mesh_with_policies<GH_policies>(mesh, uv_map, GH_policies(mesh), args);
        case Algorithm::LindstromTurk:
            typedef CGAL::Surface_mesh_simplification::LindstromTurk_cost<cgal::Mesh> LT_cost;
            typedef CGAL::Surface_mesh_simplification::LindstromTurk_placement<cgal::Mesh> LT_placement;
            return _simplify_mesh_with_cost_and_placement<LT_cost, LT_placement>(mesh, uv_map, LT_cost(), LT_placement(), args);
        }

        throw std::invalid_argument("invalid algorithm specified");
    }
}


Result simplify_mesh(const mesh::Simple&mesh, std::span<const StopCondition> stop_conditions, Options options) {
    // simplification fails with large numerical values so we normalize the values here.
    // EPECK is way too slow
    const size_t vertex_count = mesh.positions.size();
    glm::dvec3 average_position(0, 0, 0);
    for (size_t i = 0; i < vertex_count; i++) {
        average_position += mesh.positions[i] / static_cast<double>(vertex_count);
    }

    mesh::Simple normalized_mesh = mesh;
    for (size_t i = 0; i < vertex_count; i++) {
        const glm::dvec3 normalized_position = mesh.positions[i] - average_position;
        normalized_mesh.positions[i] = normalized_position;
    }

    cgal::Mesh cgal_mesh = convert::to_cgal_mesh(normalized_mesh);
    const cgal::Mesh original_mesh(cgal_mesh);

    AttachedUvPropertyMap uv_map = cgal_mesh.add_property_map<cgal::VertexDescriptor, cgal::Point2>("h:uv").first;
    for (size_t i = 0; i < mesh.uvs.size(); i++) {
        uv_map[CGAL::SM_Vertex_index(i)] = convert::to_cgal_point(mesh.uvs[i]);
    }

    const SimplificationArgs args{
        .lock_borders = options.lock_borders,
        .stop_conditions = stop_conditions};

    /*const size_t removed_edge_count = */_simplify_mesh(cgal_mesh, uv_map, options.algorithm, args);
    const double simplification_error = measure_max_absolute_error(original_mesh, cgal_mesh, 0.01);

    if (!CGAL::Polygon_mesh_processing::experimental::remove_self_intersections(cgal_mesh)) {
        LOG_WARN("Failed to remove self intersections after simplification");
    }

    mesh::Simple simplified_mesh = convert::to_simple_mesh(cgal_mesh);
    for (size_t i = 0; i < CGAL::num_vertices(cgal_mesh); i++) {
        simplified_mesh.positions[i] += average_position;
    }

    mesh::remove_isolated_vertices(simplified_mesh);

    mesh::validate(simplified_mesh);

    return Result{
        .mesh = simplified_mesh,
        .max_absolute_error = simplification_error};
}

cv::Mat simplify_texture(const cv::Mat &texture, glm::uvec2 target_resolution) {
    return rescale_texture(texture, target_resolution);
}

void simplify_mesh_texture(mesh::Simple &mesh, glm::uvec2 target_resolution) {
    if (mesh.texture.has_value()) {
        mesh.texture = simplify_texture(mesh.texture.value(), target_resolution);
    }
}

}
