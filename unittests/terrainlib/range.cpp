#include "../catch2_helpers.h"
#include "Range.h"

TEST_CASE("Range: contains value") {
    Range<int> r(2, 8);

    CHECK(r.contains(2));
    CHECK(r.contains(5));
    CHECK_FALSE(r.contains(8));
    CHECK_FALSE(r.contains(1));
    CHECK_FALSE(r.contains(9));
}

TEST_CASE("Range: is_empty") {
    CHECK(Range<int>(3, 3).is_empty());
    CHECK(Range<int>(5, 2).is_empty());
    CHECK_FALSE(Range<int>(2, 5).is_empty());
}

TEST_CASE("Range: size") {
    CHECK(Range<int>(2, 8).size() == 6);
    CHECK(Range<int>(5, 5).size() == 0);
}

TEST_CASE("Range: single-value constructor") {
    Range<int> r(5);

    CHECK(r.start == 5);
    CHECK(r.contains(5));
    CHECK_FALSE(r.contains(6));
}

TEST_CASE("Range: is_in_bounds") {
    CHECK(Range<int>(2, 8).is_in_bounds(8));
    CHECK(Range<int>(2, 8).is_in_bounds(10));
    CHECK_FALSE(Range<int>(2, 8).is_in_bounds(5));
    CHECK_FALSE(Range<int>(5, 2).is_in_bounds(10));
}

TEST_CASE("Range: is_overlapping") {
    CHECK(Range<int>(2, 8).is_overlapping(Range<int>(5, 12)));
    CHECK_FALSE(Range<int>(2, 4).is_overlapping(Range<int>(6, 8)));
    CHECK_FALSE(Range<int>(2, 4).is_overlapping(Range<int>(4, 8)));
}

TEST_CASE("Range: intersect overlapping ranges") {
    Range<int> a(2, 8);
    Range<int> b(5, 12);

    AnyRange<int> result = a.intersect(b);

    CHECK(result.start == Bound<int>::included(5));
    CHECK(result.end == Bound<int>::excluded(8));
}

TEST_CASE("Range: intersect disjoint ranges is empty") {
    Range<int> a(2, 4);
    Range<int> b(6, 8);

    CHECK(a.intersect(b).is_empty());
}

TEST_CASE("Range: intersect with RangeFull returns itself") {
    Range<int> a(2, 8);

    AnyRange<int> result = a.intersect(RangeFull{});

    CHECK(result.start == Bound<int>::included(2));
    CHECK(result.end == Bound<int>::excluded(8));
}

TEST_CASE("Range: equality") {
    CHECK(Range<int>(2, 8) == Range<int>(2, 8));
    CHECK_FALSE(Range<int>(2, 8) == Range<int>(2, 9));
}

TEST_CASE("RangeInclusive: contains value") {
    RangeInclusive<int> r(2, 8);

    CHECK(r.contains(2));
    CHECK(r.contains(8));
    CHECK_FALSE(r.contains(1));
    CHECK_FALSE(r.contains(9));
}

TEST_CASE("RangeInclusive: is_empty") {
    CHECK_FALSE(RangeInclusive<int>(3, 3).is_empty());
    CHECK(RangeInclusive<int>(5, 2).is_empty());
}

TEST_CASE("RangeFrom: contains value") {
    RangeFrom<int> r{{}, 5};

    CHECK(r.contains(5));
    CHECK(r.contains(1000));
    CHECK_FALSE(r.contains(4));
}

TEST_CASE("RangeFrom: is_empty is always false") {
    CHECK_FALSE((RangeFrom<int>{{}, 5}.is_empty()));
}

TEST_CASE("RangeTo: contains value") {
    RangeTo<int> r{{}, 5};

    CHECK(r.contains(-1000));
    CHECK(r.contains(4));
    CHECK_FALSE(r.contains(5));
}

TEST_CASE("RangeToInclusive: contains value") {
    RangeToInclusive<int> r{{}, 5};

    CHECK(r.contains(5));
    CHECK_FALSE(r.contains(6));
}

TEST_CASE("RangeFull: contains anything") {
    RangeFull r;

    CHECK(r.contains(0));
    CHECK(r.contains(-1000000));
    CHECK(r.contains(1000000));
}

TEST_CASE("RangeFull: is_empty is always false") {
    CHECK_FALSE(RangeFull{}.is_empty<int>());
}

TEST_CASE("AnyRange: constructs from and behaves like each concrete range type") {
    AnyRange<int> from_range = Range<int>(2, 8);
    CHECK(from_range.contains(5));
    CHECK_FALSE(from_range.contains(8));

    AnyRange<int> from_inclusive = RangeInclusive<int>(2, 8);
    CHECK(from_inclusive.contains(8));

    AnyRange<int> from_from = RangeFrom<int>{{}, 5};
    CHECK(from_from.contains(1000));
    CHECK_FALSE(from_from.contains(4));

    AnyRange<int> from_to = RangeTo<int>{{}, 5};
    CHECK(from_to.contains(4));
    CHECK_FALSE(from_to.contains(5));

    AnyRange<int> from_full = RangeFull{};
    CHECK(from_full.contains(1000000));
    CHECK_FALSE(from_full.is_empty());
}

TEST_CASE("AnyRange: is_empty") {
    CHECK(AnyRange<int>(Range<int>(3, 3)).is_empty());
    CHECK_FALSE(AnyRange<int>(Range<int>(2, 5)).is_empty());
    CHECK_FALSE(AnyRange<int>(RangeFrom<int>{{}, 5}).is_empty());
}

TEST_CASE("AnyRange: to_range resolves unbounded sides using the given length") {
    const AnyRange<uint32_t> full = RangeFull{};
    CHECK(full.to_range(10) == Range<uint32_t>(0, 10));

    const AnyRange<uint32_t> from = RangeFrom<uint32_t>{{}, 3};
    CHECK(from.to_range(10) == Range<uint32_t>(3, 10));

    const AnyRange<uint32_t> to = RangeTo<uint32_t>{{}, 3};
    CHECK(to.to_range(10) == Range<uint32_t>(0, 3));

    const AnyRange<uint32_t> to_inclusive = RangeToInclusive<uint32_t>{{}, 3};
    CHECK(to_inclusive.to_range(10) == Range<uint32_t>(0, 4));

    const AnyRange<uint32_t> bounded = Range<uint32_t>(2, 8);
    CHECK(bounded.to_range(100) == Range<uint32_t>(2, 8));
}
