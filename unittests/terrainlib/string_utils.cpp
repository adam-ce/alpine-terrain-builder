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
