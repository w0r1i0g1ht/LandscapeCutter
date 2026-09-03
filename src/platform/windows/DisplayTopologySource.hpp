#pragma once

#include "platform/MonitorTypes.hpp"

#include <Windows.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace lc::platform::windows {

enum class DisplayErrorCode {
    MonitorEnumerationFailed,
    DisplayConfigQueryFailed,
    DisplayConfigDeviceInfoFailed,
    DpiQueryFailed,
    TopologyValidationFailed,
};

struct DisplayError final {
    DisplayErrorCode code;
    unsigned long nativeError;
};

struct NativeMonitorRecord final {
    HMONITOR handle;
    std::wstring gdiDeviceName;
    platform::PhysicalRect desktopRect;
    platform::PhysicalRect workRect;
    std::uint32_t dpiX;
    std::uint32_t dpiY;
    bool primary;
};

struct DisplayPathRecord final {
    std::wstring gdiDeviceName;
    std::wstring friendlyName;
    std::wstring monitorDevicePath;
    LUID adapterId;
    std::uint32_t targetId;
    bool hdrEnabled;
};

struct TopologySnapshot final {
    std::vector<NativeMonitorRecord> monitors;
    std::vector<DisplayPathRecord> paths;
};

using TopologyReadResult = std::variant<TopologySnapshot, DisplayError>;

class IDisplayTopologySource {
public:
    virtual ~IDisplayTopologySource() = default;
    virtual TopologyReadResult read() = 0;
};

}  // namespace lc::platform::windows
