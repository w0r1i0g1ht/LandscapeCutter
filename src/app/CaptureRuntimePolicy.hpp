#pragma once

#include "app/CaptureCoordinator.hpp"
#include "platform/windows/GlobalHotkeyService.hpp"

namespace lc::app {

struct CaptureRuntimeState final {
    CaptureAvailability availability;
    bool captureEnabled;
    bool registerHotkey;
    bool showHotkeyConflict;
};

class CaptureRuntimePolicy final {
public:
    [[nodiscard]] static CaptureRuntimeState initial(bool supported, bool catalogReady,
                                                     bool deviceReady) noexcept;
    [[nodiscard]] static CaptureRuntimeState afterDisplayRefresh(
        CaptureRuntimeState current, bool refreshed) noexcept;
    [[nodiscard]] static CaptureRuntimeState afterHotkeyRegistration(
        CaptureRuntimeState current,
        platform::windows::HotkeyRegistrationStatus status) noexcept;
};

}  // namespace lc::app
