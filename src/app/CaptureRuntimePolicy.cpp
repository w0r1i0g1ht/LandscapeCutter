#include "app/CaptureRuntimePolicy.hpp"

namespace lc::app {

CaptureRuntimeState CaptureRuntimePolicy::initial(const bool supported, const bool catalogReady,
                                                  const bool deviceReady) noexcept {
    const auto availability = !supported ? CaptureAvailability::Unsupported
                              : !catalogReady ? CaptureAvailability::DisplayUnavailable
                              : !deviceReady ? CaptureAvailability::DeviceUnavailable
                                             : CaptureAvailability::Available;
    const bool enabled = availability == CaptureAvailability::Available;
    return {availability, enabled, enabled, false};
}

CaptureRuntimeState CaptureRuntimePolicy::afterDisplayRefresh(CaptureRuntimeState current,
                                                              const bool refreshed) noexcept {
    current.registerHotkey = false;
    current.showHotkeyConflict = false;
    if (current.availability != CaptureAvailability::Available &&
        current.availability != CaptureAvailability::DisplayUnavailable) {
        current.captureEnabled = false;
        return current;
    }

    if (!refreshed) {
        current.availability = CaptureAvailability::DisplayUnavailable;
        current.captureEnabled = false;
        return current;
    }

    current.availability = CaptureAvailability::Available;
    current.captureEnabled = true;
    current.registerHotkey = true;
    return current;
}

CaptureRuntimeState CaptureRuntimePolicy::afterHotkeyRegistration(
    CaptureRuntimeState current, const platform::windows::HotkeyRegistrationStatus status) noexcept {
    current.registerHotkey = false;
    current.showHotkeyConflict = status == platform::windows::HotkeyRegistrationStatus::Conflict;
    if (status == platform::windows::HotkeyRegistrationStatus::Failed) {
        current.availability = CaptureAvailability::DeviceUnavailable;
        current.captureEnabled = false;
    }
    return current;
}

}  // namespace lc::app
