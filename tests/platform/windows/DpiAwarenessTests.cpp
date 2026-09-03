#include "platform/windows/DpiAwareness.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("the process runs as Per-Monitor V2 before Qt exists") {
    const auto result = lc::platform::windows::ensurePerMonitorV2();
    CHECK(result.status != lc::platform::windows::DpiSetupStatus::Failed);
    CHECK(lc::platform::windows::isPerMonitorV2());
}
