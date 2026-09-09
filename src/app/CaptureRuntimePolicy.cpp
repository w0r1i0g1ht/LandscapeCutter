#include "app/CaptureRuntimePolicy.hpp"

#include <utility>

namespace lc::app {
namespace {
CaptureAvailability availabilityFor(const CaptureRuntimeState& state) noexcept {
    if (!state.captureSupported) {
        return CaptureAvailability::Unsupported;
    }
    if (!state.catalogReady) {
        return CaptureAvailability::DisplayUnavailable;
    }
    if (!state.deviceReady) {
        return CaptureAvailability::DeviceUnavailable;
    }
    return CaptureAvailability::Available;
}

void updateDerivedState(CaptureRuntimeState& state) noexcept {
    state.availability = availabilityFor(state);
    state.captureEnabled = state.availability == CaptureAvailability::Available;
}

std::optional<CaptureNoticeCode>
unavailableNotice(const CaptureAvailability availability) noexcept {
    switch (availability) {
    case CaptureAvailability::Available:
        return std::nullopt;
    case CaptureAvailability::Unsupported:
        return CaptureNoticeCode::Unsupported;
    case CaptureAvailability::DisplayUnavailable:
        return CaptureNoticeCode::DisplayUnavailable;
    case CaptureAvailability::DeviceUnavailable:
        return CaptureNoticeCode::DeviceUnavailable;
    }
    return std::nullopt;
}
} // namespace

CaptureRuntimeState CaptureRuntimePolicy::initial(const bool supported, const bool catalogReady,
                                                  const bool deviceReady) noexcept {
    CaptureRuntimeState state{
        .captureSupported = supported,
        .catalogReady = catalogReady,
        .deviceReady = deviceReady,
    };
    updateDerivedState(state);
    state.registerHotkey = state.captureEnabled;
    state.pendingNotice = unavailableNotice(state.availability);
    return state;
}

CaptureRuntimeState CaptureRuntimePolicy::afterDisplayRefresh(CaptureRuntimeState current,
                                                              const bool refreshed) noexcept {
    const auto previousAvailability = current.availability;
    current.registerHotkey = false;
    current.showHotkeyConflict = false;
    current.pendingNotice.reset();
    current.catalogReady = refreshed;
    updateDerivedState(current);
    current.registerHotkey = current.captureEnabled;
    if (current.availability != previousAvailability) {
        current.pendingNotice = unavailableNotice(current.availability);
    }
    return current;
}

CaptureRuntimeState
CaptureRuntimePolicy::afterDisplayInvalidated(CaptureRuntimeState current) noexcept {
    current.catalogReady = false;
    current.registerHotkey = false;
    current.showHotkeyConflict = false;
    current.pendingNotice.reset();
    updateDerivedState(current);
    return current;
}

CaptureRuntimeState CaptureRuntimePolicy::afterHotkeyRegistration(
    CaptureRuntimeState current,
    const platform::windows::HotkeyRegistrationStatus status) noexcept {
    const auto previousAvailability = current.availability;
    current.registerHotkey = false;
    current.showHotkeyConflict = status == platform::windows::HotkeyRegistrationStatus::Conflict;
    current.pendingNotice.reset();
    if (status == platform::windows::HotkeyRegistrationStatus::Failed) {
        current.deviceReady = false;
        updateDerivedState(current);
        if (current.availability != previousAvailability) {
            current.pendingNotice = unavailableNotice(current.availability);
        }
    }
    return current;
}

CaptureRuntimeState
CaptureRuntimePolicy::afterAvailabilityChanged(CaptureRuntimeState current,
                                               const CaptureAvailability availability) noexcept {
    current.registerHotkey = false;
    current.showHotkeyConflict = false;
    current.pendingNotice.reset();
    switch (availability) {
    case CaptureAvailability::Available:
        current.captureSupported = true;
        current.catalogReady = true;
        current.deviceReady = true;
        break;
    case CaptureAvailability::Unsupported:
        current.captureSupported = false;
        break;
    case CaptureAvailability::DisplayUnavailable:
        current.catalogReady = false;
        break;
    case CaptureAvailability::DeviceUnavailable:
        current.deviceReady = false;
        break;
    }
    updateDerivedState(current);
    return current;
}

std::optional<CaptureNoticeCode>
CaptureRuntimePolicy::takePendingNotice(CaptureRuntimeState& state) noexcept {
    return std::exchange(state.pendingNotice, std::nullopt);
}

} // namespace lc::app
