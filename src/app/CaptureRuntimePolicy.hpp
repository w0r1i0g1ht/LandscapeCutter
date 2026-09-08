#pragma once

#include "app/CaptureCoordinator.hpp"
#include "platform/windows/GlobalHotkeyService.hpp"

#include <optional>

namespace lc::app {

struct CaptureRuntimeState final {
    bool captureSupported;
    bool catalogReady;
    bool deviceReady;
    CaptureAvailability availability;
    bool captureEnabled;
    bool registerHotkey;
    bool showHotkeyConflict;
    std::optional<CaptureNoticeCode> pendingNotice;
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
    [[nodiscard]] static CaptureRuntimeState afterAvailabilityChanged(
        CaptureRuntimeState current, CaptureAvailability availability) noexcept;
    [[nodiscard]] static std::optional<CaptureNoticeCode> takePendingNotice(
        CaptureRuntimeState& state) noexcept;
};

}  // namespace lc::app
