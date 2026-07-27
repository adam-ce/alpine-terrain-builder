#include "../catch2_helpers.h"
#include "OffsetTable.h"

#include <array>
#include <vector>

TEST_CASE("OffsetTable default construction") {
    OffsetTable table;
    CHECK(table.empty());
    CHECK(table.size() == 0);
    CHECK(table.total_size() == 0);
}

TEST_CASE("OffsetTable append_length") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);
    table.append_length(2);

    CHECK(table.size() == 3);
    CHECK(table.total_size() == 10);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 3);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 3);
    CHECK(r1.end == 8);

    auto r2 = table.segment_range(2);
    CHECK(r2.begin == 8);
    CHECK(r2.end == 10);
}

TEST_CASE("OffsetTable append_lengths via span") {
    OffsetTable table;
    std::array<size_t, 3> lengths = {3, 5, 2};
    table.append_lengths(lengths);

    CHECK(table.size() == 3);
    CHECK(table.total_size() == 10);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 3);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 3);
    CHECK(r1.end == 8);

    auto r2 = table.segment_range(2);
    CHECK(r2.begin == 8);
    CHECK(r2.end == 10);
}

TEST_CASE("OffsetTable element_size") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);
    table.append_length(2);

    CHECK(table.segment_size(0) == 3);
    CHECK(table.segment_size(1) == 5);
    CHECK(table.segment_size(2) == 2);
}

TEST_CASE("OffsetTable locate") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);
    table.append_length(2);

    {
        auto result = table.locate(0);
        CHECK(result.segment == 0);
        CHECK(result.range.begin == 0);
        CHECK(result.range.end == 3);
    }
    {
        auto result = table.locate(3);
        CHECK(result.segment == 1);
        CHECK(result.range.begin == 3);
        CHECK(result.range.end == 8);
    }
    {
        auto result = table.locate(7);
        CHECK(result.segment == 1);
        CHECK(result.range.begin == 3);
        CHECK(result.range.end == 8);
    }
    {
        auto result = table.locate(8);
        CHECK(result.segment == 2);
        CHECK(result.range.begin == 8);
        CHECK(result.range.end == 10);
    }
    {
        auto result = table.locate(9);
        CHECK(result.segment == 2);
        CHECK(result.range.begin == 8);
        CHECK(result.range.end == 10);
    }
}

TEST_CASE("OffsetTable locate out of range throws") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);
    table.append_length(2);

    CHECK_THROWS_AS(table.locate(10), std::out_of_range);
    CHECK_THROWS_AS(table.locate(100), std::out_of_range);
}

TEST_CASE("OffsetTable range out of range throws") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);

    CHECK_THROWS_AS(table.segment_range(2), std::out_of_range);
    CHECK_THROWS_AS(table.segment_range(100), std::out_of_range);
}

TEST_CASE("OffsetTable set_end on last element to extend") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);
    table.append_length(2);

    table.set_end(2, 15);

    CHECK(table.total_size() == 15);
    auto r2 = table.segment_range(2);
    CHECK(r2.begin == 8);
    CHECK(r2.end == 15);
}

TEST_CASE("OffsetTable set_begin on non-first element") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);
    table.append_length(2);

    table.set_begin(1, 4);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 4);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 4);
    CHECK(r1.end == 8);
}

TEST_CASE("OffsetTable set_begin on element 0 throws") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);

    CHECK_THROWS_AS(table.set_begin(0, 1), std::out_of_range);
}

TEST_CASE("OffsetTable clear then re-add elements") {
    OffsetTable table;
    table.append_length(3);
    table.append_length(5);

    CHECK(table.size() == 2);
    CHECK(table.total_size() == 8);

    table.clear();

    CHECK(table.empty());
    CHECK(table.size() == 0);
    CHECK(table.total_size() == 0);

    table.append_length(4);
    table.append_length(6);

    CHECK(table.size() == 2);
    CHECK(table.total_size() == 10);

    auto r0 = table.segment_range(0);
    CHECK(r0.begin == 0);
    CHECK(r0.end == 4);

    auto r1 = table.segment_range(1);
    CHECK(r1.begin == 4);
    CHECK(r1.end == 10);
}
