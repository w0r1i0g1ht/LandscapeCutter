#pragma once

namespace lc::platform::windows {

enum class DpiSetupStatus { Configured, AlreadyPerMonitorV2, Failed };

struct DpiSetupResult final {
    DpiSetupStatus status;
    unsigned long nativeError;
};

DpiSetupResult ensurePerMonitorV2() noexcept;
bool isPerMonitorV2() noexcept;

}  // namespace lc::platform::windows
