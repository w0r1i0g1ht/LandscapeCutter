#include "app/AppMetadata.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("application metadata is stable") {
    CHECK(lc::app::AppMetadata::name == "LandscapeCutter");
    CHECK(lc::app::AppMetadata::organization == "LandscapeCutter");
    CHECK(lc::app::AppMetadata::version == "0.1.0-dev");
}
