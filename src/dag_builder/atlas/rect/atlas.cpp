#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include <glm/common.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <rectpack2D/finders_interface.h>

#include "atlas/rect/atlas.h"
#include "atlas/rect/PackingPlanner.h"

namespace atlas {

Plan plan(const std::span<const glm::uvec2> texture_sizes) {
    return plan(texture_sizes, PackingPlanner{});
}
Plan plan(const std::span<const glm::uvec2> texture_sizes, const Planner &planner) {
    return planner.plan(texture_sizes);
}

Texture create(const Plan &plan, const std::span<const Texture> textures) {
    if (plan.size.x == 0 || plan.size.y == 0 || plan.slots.empty() || textures.empty()) {
        return {};
    }
    ASSERT(textures.size() == plan.slots.size(), "atlas::create: textures.size() must match plan.slots.size()");

    const int atlas_w = static_cast<int>(plan.size.x);
    const int atlas_h = static_cast<int>(plan.size.y);

    // Use the first texture type; require all textures to match type.
    const int type = textures[0].type();
    Texture atlas_img(atlas_h, atlas_w, type, cv::Scalar::all(0));

    for (size_t i = 0; i < textures.size(); i++) {
        const auto &texture = textures[i];
        const auto &slot = plan.slots[i];

        if (texture.empty()) {
            continue;
        }
        ASSERT(texture.type() == type, "atlas::create: all textures must have the same cv::Mat type()");

        const glm::uvec2 texture_size(texture.cols, texture.rows);

        DEBUG_ASSERT(slot.size == texture_size,
                        "atlas::create: texture size does not match its slot size");

        const cv::Rect roi(slot.position.x,
                            slot.position.y,
                            texture_size.x,
                            texture_size.y);
        texture.copyTo(atlas_img(roi));
    }

    return atlas_img;
}

Texture plan_and_create(const std::span<const Texture> textures) {
    return plan_and_create(textures, PackingPlanner{});
}
Texture plan_and_create(const std::span<const Texture> textures, const Planner &planner) {
    std::vector<glm::uvec2> texture_sizes;
    texture_sizes.reserve(textures.size());
    for (const Texture &texture : textures) {
        texture_sizes.emplace_back(texture.cols, texture.rows);
    }

    const Plan atlas_plan = plan(texture_sizes, planner);
    return create(atlas_plan, textures);
}

namespace {
void check_uv(const glm::uvec2 &uv) {
    DEBUG_ASSERT(uv.x >= 0.0 && uv.x <= 1.0);
    DEBUG_ASSERT(uv.y >= 0.0 && uv.y <= 1.0);
}
}

void map_uvs(const Plan &plan, const uint32_t slot_index, std::span<glm::dvec2> uvs) {
    if (plan.size.x == 0 || plan.size.y == 0) {
        return;
    }
    if (slot_index >= plan.slots.size()) {
        throw std::runtime_error("atlas::map_uvs: slot_index out of range");
    }

    const auto &slot = plan.slots[slot_index];

    const glm::dvec2 atlas_size(plan.size);
    const glm::dvec2 offset(slot.position);
    const glm::dvec2 size(slot.size);

    for (auto &uv : uvs) {
        check_uv(uv);
        uv = (offset + uv * size) / atlas_size;
        check_uv(uv);
    }
}

} // namespace atlas
