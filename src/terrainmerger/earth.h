#pragma once

namespace earth {
    constexpr double largest_radius() {
        // Chimborazo
        return 6384400;
    }
    constexpr double smallest_radius() {
        // Litke Deep
        return 6351704.3;
    }
    constexpr double radius() {
        return 6371008;
    }
    constexpr glm::dvec2 radius_range() {
        return {smallest_radius(), largest_radius()};
    }
}