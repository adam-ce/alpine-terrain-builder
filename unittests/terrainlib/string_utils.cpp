/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include "../catch2_helpers.h"
#include "string_utils.h"

#include <string>
#include <string_view>

TEST_CASE("from_chars parses a plain integer", "[string_utils]") {
    CHECK(from_chars<int>(std::string_view("123")) == 123);
}

TEST_CASE("from_chars parses a negative integer", "[string_utils]") {
    CHECK(from_chars<int>(std::string_view("-5")) == -5);
}

TEST_CASE("from_chars rejects an empty string", "[string_utils]") {
    CHECK(from_chars<int>(std::string_view("")) == std::nullopt);
}

TEST_CASE("from_chars rejects non-numeric input", "[string_utils]") {
    CHECK(from_chars<int>(std::string_view("abc")) == std::nullopt);
}

TEST_CASE("from_chars rejects trailing garbage after a valid number", "[string_utils]") {
    CHECK(from_chars<int>(std::string_view("123.sfmesh")) == std::nullopt);
    CHECK(from_chars<int>(std::string_view("123abc")) == std::nullopt);
}

TEST_CASE("from_chars works with std::string overload", "[string_utils]") {
    CHECK(from_chars<int>(std::string("42")) == 42);
    CHECK(from_chars<int>(std::string("42.tmp")) == std::nullopt);
}
