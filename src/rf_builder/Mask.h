/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include <memory>
#include <span>
#include <string>
#include "RasterTransform.h"

namespace rf_builder {

class Mask {
public:
    static Expected<Mask> open(const std::string& identifier);
    ~Mask();
    Mask(Mask&&) noexcept;
    Mask& operator=(Mask&&) noexcept;
    Expected<void> select(std::span<const glm::dvec2> centres, std::span<std::uint8_t> validity) const;
    const std::vector<RasterTransform::Bounds>& bounds() const;

private:
    struct Data;
    explicit Mask(std::unique_ptr<Data> data);
    std::unique_ptr<Data> m_data;
};

} // namespace rf_builder
