#include "app/CaptureRuntimePolicy.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {
using lc::app::CaptureAvailability;
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

TEST_CASE("device unavailable remains disabled across display events") {
    auto state = CaptureRuntimePolicy::initial(true, true, false);

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

}  // namespace
