#include "platform/windows/DpiAwareness.hpp"

#include <Windows.h>

namespace lc::platform::windows {

bool isPerMonitorV2() noexcept {
    return AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
                                        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
}

DpiSetupResult ensurePerMonitorV2() noexcept {
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE) {
        return {DpiSetupStatus::Configured, ERROR_SUCCESS};
    }

    const unsigned long nativeError = GetLastError();
    if (nativeError == ERROR_ACCESS_DENIED && isPerMonitorV2()) {
        return {DpiSetupStatus::AlreadyPerMonitorV2, nativeError};
    }

    return {DpiSetupStatus::Failed, nativeError};
}

}  // namespace lc::platform::windows
