#include "app/LaunchOptions.hpp"

using namespace std::string_view_literals;

#include <array>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("normal launch is the default") {
    constexpr std::array arguments{"LandscapeCutter"sv};
    CHECK(lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::Normal);
}

TEST_CASE("smoke-test flag selects smoke mode") {
    constexpr std::array arguments{"LandscapeCutter"sv, "--smoke-test"sv};
    CHECK(lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::SmokeTest);
}

TEST_CASE("unrelated arguments do not select smoke mode") {
    constexpr std::array arguments{"LandscapeCutter"sv, "--verbose"sv};
    CHECK(lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::Normal);
}
