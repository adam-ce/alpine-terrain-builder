#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <rectpack2D/finders_interface.h>

#include "atlas/rect/PackingPlanner.h"

namespace atlas {

Plan PackingPlanner::plan(const std::span<const glm::uvec2> texture_sizes) const {
    constexpr bool allow_flip = false;
    using spaces_type = rectpack2D::empty_spaces<allow_flip, rectpack2D::default_empty_spaces>;
    using rect_type = rectpack2D::output_rect_t<spaces_type>;

    struct RectAndIndex {
        rect_type rect{};
        uint32_t index = 0;

        auto &get_rect() {
            return rect;
        }
        const auto &get_rect() const {
            return rect;
        }
    };

    if (texture_sizes.empty()) {
        return Plan{
            .size = glm::uvec2(0u, 0u),
            .slots = {}};
    }

    uint32_t max_side = 0u;
    for (const glm::uvec2 &size : texture_sizes) {
        max_side = std::max(max_side, glm::compMax(size));
    }

    // If everything is 0x0, keep atlas 0x0 (nothing to pack).
    if (max_side == 0u) {
        std::vector<Rect2ui> slots(texture_sizes.size(), Rect2ui{
                                                             .position = glm::uvec2(0u, 0u),
                                                             .size = glm::uvec2(0u, 0u),
                                                         });

        return Plan{
            .size = glm::uvec2(0u, 0u),
            .slots = std::move(slots)};
    }

    std::vector<RectAndIndex> rects;
    rects.reserve(texture_sizes.size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(texture_sizes.size()); ++i) {
        const glm::uvec2 &size = texture_sizes[i];
        rects.push_back(RectAndIndex{
            .rect = rectpack2D::rect_xywh(0, 0, size.x, size.y),
            .index = i});
    }

    std::optional<rectpack2D::rect_wh> result;

    // Expand search square until we succeed.
    for (uint32_t attempt = 0; attempt < 256u; attempt++) {
        bool any_unsuccessful = false;

        auto report_successful = [&](rect_type &) {
            return rectpack2D::callback_result::CONTINUE_PACKING;
        };

        auto report_unsuccessful = [&](rect_type &) {
            any_unsuccessful = true;
            return rectpack2D::callback_result::ABORT_PACKING;
        };

        const int discard_step = 3; // speed vs quality tradeoff

        result = rectpack2D::find_best_packing<spaces_type>(
            rects,
            rectpack2D::make_finder_input(
                static_cast<int>(max_side),
                discard_step,
                report_successful,
                report_unsuccessful,
                rectpack2D::flipping_option::DISABLED));

        if (result && !any_unsuccessful) {
            break;
        }

        // Grow ~20%
        max_side = static_cast<uint32_t>(static_cast<float>(max_side) * 1.2f) + 1u;
    }

    if (!result) {
        throw std::runtime_error("atlas::PackingPlanner::plan: rectpack2D failed to find a packing");
    }

    Plan out;
    out.size = glm::uvec2(result->w, result->h);

    out.slots.resize(rects.size());

    for (const auto &r : rects) {
        out.slots[r.index] = Rect2ui{
            .position = glm::uvec2(r.rect.x, r.rect.y),
            .size = glm::uvec2(r.rect.w, r.rect.h),
        };
    }

    return out;
}

} // namespace atlas
