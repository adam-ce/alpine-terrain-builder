/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <span>
#include <functional>

#include "mesh/SimpleMesh.h"
#include "atlas/LayoutPlanner.h"
#include "atlas/Layout.h"

namespace atlas {

class HorizontalStripLayoutPlanner : public LayoutPlanner {
public:
    Layout plan(const std::span<const std::reference_wrapper<const SimpleMesh>> meshes) const override {
        Layout layout;
        glm::uvec2 cursor(0, 0);
        uint32_t max_height = 0;

        for (size_t i = 0; i < meshes.size(); i++) {
            const SimpleMesh &mesh = meshes[i];
            if (!mesh.texture.has_value()) {
                continue;
            }

            const cv::Mat &tex = *mesh.texture;

            const Layout::Slot slot{
                .mesh_index = i,
                .pixel_position = cursor,
                .uv_space = {} // to be filled later
            };
            layout.slots.push_back(slot);

            cursor.x += tex.cols;
            max_height = std::max(max_height, static_cast<uint32_t>(tex.rows));
        }

        if (layout.slots.empty()) {
            layout.atlas_size = glm::uvec2(1, 1);
            return layout;
        }

        layout.atlas_size = glm::uvec2(cursor.x, max_height);

        // compute uv rects
        for (auto &slot : layout.slots) {
            const SimpleMesh &mesh = meshes[slot.mesh_index].get();
            const cv::Mat &tex = *mesh.texture;

            const double u0 = static_cast<double>(slot.pixel_position.x) / layout.atlas_size.x;
            const double v0 = static_cast<double>(slot.pixel_position.y) / layout.atlas_size.y;
            const double u1 = static_cast<double>(slot.pixel_position.x + tex.cols) / layout.atlas_size.x;
            const double v1 = static_cast<double>(slot.pixel_position.y + tex.rows) / layout.atlas_size.y;

            slot.uv_space = Rect2d{
                {u0, v0},
                {u1 - u0, v1 - v0}};
        }

        return layout;
    }
};

} // namespace atlas
