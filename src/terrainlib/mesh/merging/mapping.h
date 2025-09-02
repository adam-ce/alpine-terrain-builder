#pragma once

#include <span>
#include <functional>

#include "mesh/SimpleMesh.h"
#include "mesh/merging/VertexDeduplicate.h"
#include "mesh/merging/VertexId.h"
#include "mesh/merging/VertexMapping.h"
#include "mesh/merging/helpers.h"

namespace mesh::merging {

// VertexMapping create_connecting_mapping(std::span<const SimpleMesh> meshes);

namespace detail {
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

template <typename Mode>
VertexMapping create_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const CreateOptions<Mode> options = create_options()) {
    switch (meshes.size()) {
        case 0: 
            return {};
        case 1:
            return VertexMapping::identity(meshes[0].get().vertex_count());
        default:
            if constexpr (std::is_same_v<Mode, EstimateEpsilon>) {
                auto deduplicate = make_default_deduplicate(meshes);
                LOG_TRACE("Creating merge mapping with default epsilon = {}", deduplicate.epsilon());
                return detail::create_mapping(meshes, {options.get_only_consider_boundary(), deduplicate});
            } else if constexpr (std::is_same_v<Mode, ProvidedEpsilon>) {
                const double epsilon = options.get_mode().value;
                LOG_TRACE("Creating merge mapping with epsilon = {}", epsilon);
                auto deduplicate = make_epsilon_deduplicate(meshes, epsilon);
                return detail::create_mapping(meshes, {options.get_only_consider_boundary(), deduplicate});
            } else if constexpr (std::is_same_v<Mode, ProvidedDeduplicate>) {
                return detail::create_mapping(meshes, {options.get_only_consider_boundary(), options.get_mode().deduplicate});
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

inline SimpleMesh apply_mapping(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const VertexMapping &mapping,
    const ApplyOptions options = apply_options()) {
    return detail::apply_mapping(meshes, mapping, detail::ResolvedApplyOptions {
        .deduplicate_triangles = options.get_deduplicate_triangles(),
        .merge_uvs = options.get_merge_uvs()
    });
}

} // namespace mesh::merging
