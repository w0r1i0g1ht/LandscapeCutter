#include <catch2/catch_test_macros.hpp>

#include "platform/MonitorTypes.hpp"

TEST_CASE("physical rectangles support negative monitor coordinates") {
    const lc::platform::PhysicalRect rect{-1920, 0, 0, 1080};
    CHECK(lc::platform::width(rect) == 1920);
    CHECK(lc::platform::height(rect) == 1080);
    CHECK(lc::platform::contains(rect, {-1, 1079}));
}

TEST_CASE("physical rectangles use half-open right and bottom edges") {
    const lc::platform::PhysicalRect rect{0, 0, 2560, 1440};
    CHECK(lc::platform::contains(rect, {0, 0}));
    CHECK_FALSE(lc::platform::contains(rect, {2560, 0}));
    CHECK_FALSE(lc::platform::contains(rect, {0, 1440}));
}

TEST_CASE("inverted or empty physical rectangles are invalid") {
    CHECK_FALSE(lc::platform::isValid({0, 0, 0, 100}));
    CHECK_FALSE(lc::platform::isValid({50, 50, 49, 51}));
}
