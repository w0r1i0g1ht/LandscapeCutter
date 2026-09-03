#pragma once

#include "platform/MonitorTypes.hpp"

#include <Windows.h>

#include <cstdint>
#include <string>

namespace lc::platform::windows {

struct MonitorDescriptor final {
    MonitorId id;
    std::wstring displayName;
    std::wstring gdiDeviceName;
    HMONITOR nativeHandle;
    PhysicalRect desktopRect;
    PhysicalRect workRect;
    std::uint32_t dpiX;
    std::uint32_t dpiY;
    double scaleFactor;
    bool primary;
    bool hdrEnabled;
    LUID adapterId;
    std::uint32_t targetId;
    std::uint64_t catalogGeneration;
};

}  // namespace lc::platform::windows
