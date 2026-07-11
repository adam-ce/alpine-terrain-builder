#pragma once

#include <functional>

#include "mesh/SimpleMesh.h"
#include "mesh/merging/VertexDeduplicate.h"
#include "mesh/merging/VertexId.h"
#include "mesh/merging/VertexMapping.h"
#include "mesh/merging/helpers.h"
#include "type_utils.h"

namespace mesh::merging {

namespace detail {
template<typename T, std::size_t N, std::size_t... I> constexpr auto span_to_refs_array_impl(std::span<T, N> s, std::index_sequence<I...>) {
    return std::array<std::reference_wrapper<T>, N>{std::ref(s[I])...};
}

template <typename T, std::size_t Extent>
constexpr auto span_to_refs(std::span<T, Extent> s) {
    using U = std::remove_cv_t<T>;
    if constexpr (is_specialization_of_v<U, std::reference_wrapper>) {
        return s;
    } else if constexpr (Extent == std::dynamic_extent) {
        std::vector<std::reference_wrapper<T>> out;
        out.reserve(s.size());
        for (T &x : s) {
            out.emplace_back(x);
        }
        return out;
    } else {
        return span_to_refs_array_impl<T, Extent>(s, std::make_index_sequence<Extent>{});
    }
}

struct ResolvedCreateOptions {
    bool only_consider_boundary;
    VertexDeduplicate<3, double, VertexId>& deduplicate;
};

VertexMapping create_mapping(
    std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const ResolvedCreateOptions options);

struct ResolvedApplyOptions {
    bool deduplicate_triangles;
    bool merge_uvs;
};

SimpleMesh apply_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const VertexMapping &mapping,
    const ResolvedApplyOptions options);
}

struct EstimateEpsilon {};
struct ProvidedEpsilon {
    double value;
};
struct ProvidedDeduplicate {
    VertexDeduplicate<3, double, VertexId>& deduplicate;
};

template <typename Mode = EstimateEpsilon>
struct CreateOptions {
private:
    bool _only_consider_boundary = false;
    Mode _mode{};

public:
    template <typename T = Mode, typename = std::enable_if_t<std::is_same_v<T, EstimateEpsilon>>>
    static CreateOptions<EstimateEpsilon> defaults() {
        return {};
    }

    template <typename T = Mode, typename = std::enable_if_t<std::is_same_v<T, EstimateEpsilon>>>
    auto epsilon(const double epsilon) {
        return CreateOptions<ProvidedEpsilon>{this->_only_consider_boundary, ProvidedEpsilon{epsilon}};
    }

    template <typename T = Mode, typename = std::enable_if_t<std::is_same_v<T, EstimateEpsilon>>>
    auto deduplicate(VertexDeduplicate<3, double, VertexId> &deduplicate) {
        return CreateOptions<ProvidedDeduplicate>{this->_only_consider_boundary, ProvidedDeduplicate{deduplicate}};
    }

    bool get_only_consider_boundary() const {
        return this->_only_consider_boundary;
    }
    auto &only_consider_boundary(const bool value) {
        this->_only_consider_boundary = value;
        return *this;
    }

    Mode get_mode() const {
        return this->_mode;
    }

private:
    // Private constructor for mode transitions
    CreateOptions() = default;
    CreateOptions(bool only_consider_boundary, Mode mode) : _only_consider_boundary(only_consider_boundary), _mode(mode) {}
    friend struct CreateOptions<EstimateEpsilon>;
};

inline CreateOptions<EstimateEpsilon> create_options() {
    return CreateOptions<EstimateEpsilon>::defaults();
}

template <typename Meshes, typename Mode = EstimateEpsilon>
VertexMapping create_mapping(
    const Meshes meshes,
    const CreateOptions<Mode> options = create_options()) {
    const auto mesh_span = detail::span_to_refs(std::span(meshes));
    switch (meshes.size()) {
        case 0: 
            return {};
        case 1:
            return VertexMapping::identity(mesh_span[0].get().vertex_count());
        default:
            if constexpr (std::is_same_v<Mode, EstimateEpsilon>) {
                auto deduplicate = make_default_deduplicate(mesh_span);
                LOG_TRACE("Creating merge mapping with default epsilon = {}", deduplicate.epsilon());
                return detail::create_mapping(mesh_span, {options.get_only_consider_boundary(), deduplicate});
            } else if constexpr (std::is_same_v<Mode, ProvidedEpsilon>) {
                const double epsilon = options.get_mode().value;
                LOG_TRACE("Creating merge mapping with epsilon = {}", epsilon);
                auto deduplicate = make_epsilon_deduplicate(mesh_span, epsilon);
                return detail::create_mapping(mesh_span, {options.get_only_consider_boundary(), deduplicate});
            } else if constexpr (std::is_same_v<Mode, ProvidedDeduplicate>) {
                return detail::create_mapping(mesh_span, {options.get_only_consider_boundary(), options.get_mode().deduplicate});
            } else {
                UNREACHABLE();
            }
    }
}

struct ApplyOptions {
public:
    static ApplyOptions defaults() {
        return {};
    }
    
    bool get_deduplicate_triangles() const {
        return this->_deduplicate_triangles;
    }
    ApplyOptions &deduplicate_triangles(const bool value) {
        this->_deduplicate_triangles = value;
        return *this;
    }

    bool get_merge_uvs() const {
        return this->_merge_uvs;
    }
    ApplyOptions &merge_uvs(const bool value) {
        this->_merge_uvs = value;
        return *this;
    }

private:
    bool _deduplicate_triangles = true;
    bool _merge_uvs = false;
};

inline ApplyOptions apply_options() {
    return ApplyOptions::defaults();
}

template <typename Meshes>
inline SimpleMesh apply_mapping(
    const Meshes meshes,
    const VertexMapping &mapping,
    const ApplyOptions options = apply_options()) {
    const auto mesh_span = detail::span_to_refs(std::span(meshes));
    return detail::apply_mapping(mesh_span, mapping, detail::ResolvedApplyOptions {
        .deduplicate_triangles = options.get_deduplicate_triangles(),
        .merge_uvs = options.get_merge_uvs()
    });
}

} // namespace mesh::merging
                                                                                                                