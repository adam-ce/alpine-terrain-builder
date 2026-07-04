#include "../catch2_helpers.h"
#include "SegmentedBuffer.h"

using SB = SegmentedBuffer<int>;

TEST_CASE("SegmentedBuffer default construction") {
    SB buf;

    CHECK(buf.segment_count() == 1);
    CHECK(buf.total_size() == 0);
    CHECK(buf.segment_size(0) == 0);
}

TEST_CASE("SegmentedBuffer construct from vector") {
    std::vector<int> data = {10, 20, 30};
    SB buf(data);

    REQUIRE(buf.segment_count() == 1);
    CHECK(buf.total_size() == 3);
    REQUIRE(buf.segment_size(0) == 3);
    CHECK(buf(0, 0) == 10);
    CHECK(buf(0, 1) == 20);
    CHECK(buf(0, 2) == 30);
}

TEST_CASE("SegmentedBuffer push_to_last_segment") {
    SB buf;

    buf.push_to_last_segment(1);
    buf.push_to_last_segment(2);
    buf.push_to_last_segment(3);

    REQUIRE(buf.segment_count() == 1);
    REQUIRE(buf.segment_size(0) == 3);
    CHECK(buf(0, 0) == 1);
    CHECK(buf(0, 1) == 2);
    CHECK(buf(0, 2) == 3);
}

TEST_CASE("SegmentedBuffer start_new_segment then push_to_last_segment") {
    SB buf;

    buf.push_to_last_segment(10);
    buf.push_to_last_segment(20);

    buf.start_new_segment();

    buf.push_to_last_segment(30);
    buf.push_to_last_segment(40);
    buf.push_to_last_segment(50);

    REQUIRE(buf.segment_count() == 2);
    REQUIRE(buf.segment_size(0) == 2);
    REQUIRE(buf.segment_size(1) == 3);
    CHECK(buf.total_size() == 5);
    CHECK(buf(0, 0) == 10);
    CHECK(buf(0, 1) == 20);
    CHECK(buf(1, 0) == 30);
    CHECK(buf(1, 1) == 40);
    CHECK(buf(1, 2) == 50);
}

TEST_CASE("SegmentedBuffer push_new_segment with size") {
    SB buf;

    buf.push_new_segment(4, 99);

    REQUIRE(buf.segment_count() == 1);
    REQUIRE(buf.segment_size(0) == 4);
    CHECK(buf.total_size() == 4);
    CHECK(buf(0, 0) == 99);
    CHECK(buf(0, 1) == 99);
    CHECK(buf(0, 2) == 99);
    CHECK(buf(0, 3) == 99);
}

TEST_CASE("SegmentedBuffer push_new_segment from range") {
    SB buf;

    buf.push_to_last_segment(1);

    std::vector<int> data = {100, 200, 300};
    buf.push_new_segment(data);

    REQUIRE(buf.segment_count() == 2);
    REQUIRE(buf.segment_size(0) == 1);
    REQUIRE(buf.segment_size(1) == 3);
    REQUIRE(buf.total_size() == 4);
    CHECK(buf(0, 0) == 1);
    CHECK(buf(1, 0) == 100);
    CHECK(buf(1, 1) == 200);
    CHECK(buf(1, 2) == 300);
}

TEST_CASE("SegmentedBuffer init with segment sizes") {
    SB buf;

    std::vector<size_t> sizes = {2, 3, 1};
    buf.init(sizes, 42);

    REQUIRE(buf.segment_count() == 3);
    REQUIRE(buf.segment_size(0) == 2);
    REQUIRE(buf.segment_size(1) == 3);
    REQUIRE(buf.segment_size(2) == 1);
    REQUIRE(buf.total_size() == 6);
    CHECK(buf(0, 0) == 42);
    CHECK(buf(1, 2) == 42);
    CHECK(buf(2, 0) == 42);
}

TEST_CASE("SegmentedBuffer operator() access across segments") {
    SB buf;

    std::vector<size_t> sizes = {2, 3};
    buf.init(sizes, 0);

    buf(0, 0) = 10;
    buf(0, 1) = 20;
    buf(1, 0) = 30;
    buf(1, 1) = 40;
    buf(1, 2) = 50;

    CHECK(buf(0, 0) == 10);
    CHECK(buf(0, 1) == 20);
    CHECK(buf(1, 0) == 30);
    CHECK(buf(1, 1) == 40);
    CHECK(buf(1, 2) == 50);
}

TEST_CASE("SegmentedBuffer segment() span view") {
    SB buf;

    buf.push_to_last_segment(5);
    buf.push_to_last_segment(6);

    buf.start_new_segment();

    buf.push_to_last_segment(7);

    auto seg0 = buf.segment(0);
    auto seg1 = buf.segment(1);

    REQUIRE(seg0.size() == 2);
    CHECK(seg0[0] == 5);
    CHECK(seg0[1] == 6);

    REQUIRE(seg1.size() == 1);
    CHECK(seg1[0] == 7);
}

TEST_CASE("SegmentedBuffer flat() view") {
    SB buf;

    buf.push_to_last_segment(1);
    buf.push_to_last_segment(2);

    buf.start_new_segment();

    buf.push_to_last_segment(3);
    buf.push_to_last_segment(4);
    buf.push_to_last_segment(5);

    auto flat = buf.flat();

    REQUIRE(flat.size() == 5);
    CHECK(flat[0] == 1);
    CHECK(flat[1] == 2);
    CHECK(flat[2] == 3);
    CHECK(flat[3] == 4);
    CHECK(flat[4] == 5);
}

TEST_CASE("SegmentedBuffer reset") {
    SB buf;

    buf.push_to_last_segment(1);
    buf.push_to_last_segment(2);
    buf.start_new_segment();
    buf.push_to_last_segment(3);

    REQUIRE(buf.segment_count() == 2);
    REQUIRE(buf.total_size() == 3);

    buf.reset();

    REQUIRE(buf.segment_count() == 1);
    REQUIRE(buf.total_size() == 0);
    REQUIRE(buf.segment_size(0) == 0);
}

TEST_CASE("SegmentedBuffer multiple segments with different sizes and last_segment") {
    SB buf;

    buf.push_to_last_segment(10);

    buf.start_new_segment();
    buf.push_to_last_segment(20);
    buf.push_to_last_segment(21);

    buf.start_new_segment();
    buf.push_to_last_segment(30);
    buf.push_to_last_segment(31);
    buf.push_to_last_segment(32);

    REQUIRE(buf.segment_count() == 3);
    REQUIRE(buf.segment_size(0) == 1);
    REQUIRE(buf.segment_size(1) == 2);
    REQUIRE(buf.segment_size(2) == 3);
    REQUIRE(buf.total_size() == 6);

    auto last = buf.last_segment();
    REQUIRE(last.size() == 3);
    CHECK(last[0] == 30);
    CHECK(last[1] == 31);
    CHECK(last[2] == 32);
}
