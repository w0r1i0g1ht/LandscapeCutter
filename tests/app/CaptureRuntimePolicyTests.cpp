#include "app/CaptureRuntimePolicy.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {
using lc::app::CaptureAvailability;
using lc::app::CaptureNoticeCode;
using lc::app::CaptureRuntimePolicy;
using lc::platform::windows::HotkeyRegistrationStatus;

TEST_CASE("a failed display refresh becomes unavailable and a later refresh recovers") {
    auto state = CaptureRuntimePolicy::initial(true, true, true);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    CHECK(state.availability == CaptureAvailability::DisplayUnavailable);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, true);
    CHECK(state.availability == CaptureAvailability::Available);
    CHECK(state.captureEnabled);
    CHECK(state.registerHotkey);
}

TEST_CASE("display invalidation blocks capture before the debounced refresh") {
    auto state = CaptureRuntimePolicy::initial(true, true, true);

    state = CaptureRuntimePolicy::afterDisplayInvalidated(state);

    CHECK_FALSE(state.catalogReady);
    CHECK(state.availability == CaptureAvailability::DisplayUnavailable);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);
    CHECK_FALSE(CaptureRuntimePolicy::takePendingNotice(state).has_value());
}

TEST_CASE("device unavailable remains disabled across display events") {
    auto state = CaptureRuntimePolicy::initial(true, true, false);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    state = CaptureRuntimePolicy::afterDisplayRefresh(state, true);

    CHECK(state.availability == CaptureAvailability::DeviceUnavailable);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);
}

TEST_CASE("combined catalog and device failure cannot recover through display refresh") {
    auto state = CaptureRuntimePolicy::initial(true, false, false);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    CHECK(state.availability == CaptureAvailability::DisplayUnavailable);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, true);
    CHECK(state.availability == CaptureAvailability::DeviceUnavailable);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);
}

TEST_CASE("initial policy preserves each capture prerequisite independently") {
    const auto available = CaptureRuntimePolicy::initial(true, true, true);
    CHECK(available.captureSupported);
    CHECK(available.catalogReady);
    CHECK(available.deviceReady);

    const auto unsupported = CaptureRuntimePolicy::initial(false, true, true);
    CHECK_FALSE(unsupported.captureSupported);
    CHECK(unsupported.catalogReady);
    CHECK(unsupported.deviceReady);

    const auto noCatalogOrDevice = CaptureRuntimePolicy::initial(true, false, false);
    CHECK(noCatalogOrDevice.captureSupported);
    CHECK_FALSE(noCatalogOrDevice.catalogReady);
    CHECK_FALSE(noCatalogOrDevice.deviceReady);
}

TEST_CASE("coordinator device unavailability cannot be recovered by display refresh") {
    auto state = CaptureRuntimePolicy::initial(true, true, true);
    state = CaptureRuntimePolicy::afterAvailabilityChanged(
        state, CaptureAvailability::DeviceUnavailable);

    CHECK_FALSE(state.deviceReady);
    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    state = CaptureRuntimePolicy::afterDisplayRefresh(state, true);

    CHECK(state.availability == CaptureAvailability::DeviceUnavailable);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);
}

TEST_CASE("failed hotkey registration cannot be recovered by display refresh") {
    auto state = CaptureRuntimePolicy::afterHotkeyRegistration(
        CaptureRuntimePolicy::initial(true, true, true),
        HotkeyRegistrationStatus::Failed);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    state = CaptureRuntimePolicy::afterDisplayRefresh(state, true);

    CHECK(state.availability == CaptureAvailability::DeviceUnavailable);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);
}

TEST_CASE("unsupported capture remains disabled across display events") {
    auto state = CaptureRuntimePolicy::initial(false, true, true);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    state = CaptureRuntimePolicy::afterDisplayRefresh(state, true);

    CHECK(state.availability == CaptureAvailability::Unsupported);
    CHECK_FALSE(state.captureEnabled);
    CHECK_FALSE(state.registerHotkey);
}

TEST_CASE("refresh hotkey outcomes keep conflict distinct from failed registration") {
    const auto refreshed = CaptureRuntimePolicy::afterDisplayRefresh(
        CaptureRuntimePolicy::initial(true, false, true), true);

    const auto registered = CaptureRuntimePolicy::afterHotkeyRegistration(
        refreshed, HotkeyRegistrationStatus::Registered);
    CHECK(registered.availability == CaptureAvailability::Available);
    CHECK(registered.captureEnabled);
    CHECK_FALSE(registered.showHotkeyConflict);

    const auto conflict = CaptureRuntimePolicy::afterHotkeyRegistration(
        refreshed, HotkeyRegistrationStatus::Conflict);
    CHECK(conflict.availability == CaptureAvailability::Available);
    CHECK(conflict.captureEnabled);
    CHECK(conflict.showHotkeyConflict);

    const auto failed = CaptureRuntimePolicy::afterHotkeyRegistration(
        refreshed, HotkeyRegistrationStatus::Failed);
    CHECK(failed.availability == CaptureAvailability::DeviceUnavailable);
    CHECK_FALSE(failed.captureEnabled);
    CHECK_FALSE(failed.showHotkeyConflict);
}

TEST_CASE("startup unavailability exposes one structured notice to the assembly boundary") {
    struct Case final {
        bool supported;
        bool catalogReady;
        bool deviceReady;
        CaptureNoticeCode expected;
    };
    const Case cases[]{
        {false, true, true, CaptureNoticeCode::Unsupported},
        {true, false, true, CaptureNoticeCode::DisplayUnavailable},
        {true, true, false, CaptureNoticeCode::DeviceUnavailable},
    };

    for (const auto& testCase : cases) {
        auto state = CaptureRuntimePolicy::initial(
            testCase.supported, testCase.catalogReady, testCase.deviceReady);

        CHECK(state.pendingNotice == testCase.expected);
        CHECK(CaptureRuntimePolicy::takePendingNotice(state) == testCase.expected);
        CHECK_FALSE(CaptureRuntimePolicy::takePendingNotice(state).has_value());
    }
}

TEST_CASE("display refresh failure notifies only when unavailable state changes") {
    auto state = CaptureRuntimePolicy::initial(true, true, true);
    CHECK_FALSE(CaptureRuntimePolicy::takePendingNotice(state).has_value());

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    CHECK(CaptureRuntimePolicy::takePendingNotice(state) ==
          CaptureNoticeCode::DisplayUnavailable);

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    CHECK_FALSE(CaptureRuntimePolicy::takePendingNotice(state).has_value());

    state = CaptureRuntimePolicy::afterDisplayRefresh(state, true);
    CHECK_FALSE(CaptureRuntimePolicy::takePendingNotice(state).has_value());
    state = CaptureRuntimePolicy::afterDisplayRefresh(state, false);
    CHECK(CaptureRuntimePolicy::takePendingNotice(state) ==
          CaptureNoticeCode::DisplayUnavailable);
}

TEST_CASE("newly revealed device and hotkey failures expose device unavailable notices") {
    auto combined = CaptureRuntimePolicy::initial(true, false, false);
    REQUIRE(CaptureRuntimePolicy::takePendingNotice(combined) ==
            CaptureNoticeCode::DisplayUnavailable);

    combined = CaptureRuntimePolicy::afterDisplayRefresh(combined, true);
    CHECK(combined.availability == CaptureAvailability::DeviceUnavailable);
    CHECK(CaptureRuntimePolicy::takePendingNotice(combined) ==
          CaptureNoticeCode::DeviceUnavailable);

    auto hotkeyFailed = CaptureRuntimePolicy::afterHotkeyRegistration(
        CaptureRuntimePolicy::initial(true, true, true),
        HotkeyRegistrationStatus::Failed);
    CHECK(CaptureRuntimePolicy::takePendingNotice(hotkeyFailed) ==
          CaptureNoticeCode::DeviceUnavailable);
}

TEST_CASE("coordinator availability synchronization does not duplicate its own notice") {
    auto state = CaptureRuntimePolicy::afterAvailabilityChanged(
        CaptureRuntimePolicy::initial(true, true, true),
        CaptureAvailability::DeviceUnavailable);

    CHECK(state.availability == CaptureAvailability::DeviceUnavailable);
    CHECK_FALSE(CaptureRuntimePolicy::takePendingNotice(state).has_value());
}

}  // namespace
