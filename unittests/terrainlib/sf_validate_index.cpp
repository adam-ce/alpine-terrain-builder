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

#include <catch2/catch_test_macros.hpp>

#include "octree/StoreTraits.h"
#include "sf/validate_index.h"

TEST_CASE("Structura Fundamentalis validation accepts Leaf and Virtual nodes", "[sf][store]")
{
    store::Index<octree::StoreTraits> index;
    const octree::Id leaf = octree::Id::root().child(2).value().child(5).value();
    REQUIRE(index.add(leaf).has_value());

    CHECK(sf::validate_index(index).has_value());
}

TEST_CASE("Structura Fundamentalis validation reports the first Inner node", "[sf][store]")
{
    store::Index<octree::StoreTraits> index;
    const octree::Id root = octree::Id::root();
    const octree::Id child = root.child(3).value();
    REQUIRE(index.add(root).has_value());
    REQUIRE(index.add(child).has_value());

    const auto result = sf::validate_index(index);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == Error::Code::CorruptData);
    CHECK(result.error().to_string().contains(root.to_string()));
}
