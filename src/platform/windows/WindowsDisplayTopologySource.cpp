#include "platform/windows/WindowsDisplayTopologySource.hpp"

#include <ShellScalingApi.h>
#include <Windows.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace lc::platform::windows {
namespace {

struct MonitorEnumerationContext final {
    std::vector<NativeMonitorRecord> monitors;
    std::optional<DisplayError> error;
};

unsigned long errorOrGenericFailure() {
    const unsigned long error = GetLastError();
    return error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
}

BOOL CALLBACK collectMonitor(HMONITOR handle, HDC, LPRECT, LPARAM contextValue) {
    auto& context = *reinterpret_cast<MonitorEnumerationContext*>(contextValue);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(handle, &info)) {
        context.error = DisplayError{DisplayErrorCode::MonitorEnumerationFailed,
                                     errorOrGenericFailure()};
        return FALSE;
    }

    UINT dpiX = 0;
    UINT dpiY = 0;
    const HRESULT dpiResult = GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    if (FAILED(dpiResult)) {
        context.error = DisplayError{DisplayErrorCode::DpiQueryFailed,
                                     static_cast<unsigned long>(dpiResult)};
        return FALSE;
    }
    if (dpiX == 0 || dpiY == 0) {
        context.error = DisplayError{DisplayErrorCode::DpiQueryFailed, ERROR_INVALID_DATA};
        return FALSE;
    }

    context.monitors.push_back(NativeMonitorRecord{
        handle,
        info.szDevice,
        platform::PhysicalRect{info.rcMonitor.left,
                               info.rcMonitor.top,
                               info.rcMonitor.right,
                               info.rcMonitor.bottom},
        platform::PhysicalRect{
            info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom},
        dpiX,
        dpiY,
        (info.dwFlags & MONITORINFOF_PRIMARY) != 0,
    });
    return TRUE;
}

std::variant<std::vector<DISPLAYCONFIG_PATH_INFO>, DisplayError> readActivePaths() {
    constexpr UINT32 queryFlags = QDC_ONLY_ACTIVE_PATHS;
    for (;;) {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        const LONG sizeResult =
            GetDisplayConfigBufferSizes(queryFlags, &pathCount, &modeCount);
        if (sizeResult != ERROR_SUCCESS) {
            return DisplayError{DisplayErrorCode::DisplayConfigQueryFailed,
                                static_cast<unsigned long>(sizeResult)};
        }

        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        const LONG queryResult = QueryDisplayConfig(queryFlags,
                                                    &pathCount,
                                                    paths.data(),
                                                    &modeCount,
                                                    modes.data(),
                                                    nullptr);
        if (queryResult == ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }
        if (queryResult != ERROR_SUCCESS) {
            return DisplayError{DisplayErrorCode::DisplayConfigQueryFailed,
                                static_cast<unsigned long>(queryResult)};
        }
        paths.resize(pathCount);
        return paths;
    }
}

std::variant<std::vector<DisplayPathRecord>, DisplayError> describePaths(
    const std::vector<DISPLAYCONFIG_PATH_INFO>& nativePaths) {
    std::vector<DisplayPathRecord> paths;
    paths.reserve(nativePaths.size());
    for (const auto& nativePath : nativePaths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};
        sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sourceName.header.size = sizeof(sourceName);
        sourceName.header.adapterId = nativePath.sourceInfo.adapterId;
        sourceName.header.id = nativePath.sourceInfo.id;
        const LONG sourceResult = DisplayConfigGetDeviceInfo(&sourceName.header);
        if (sourceResult != ERROR_SUCCESS) {
            return DisplayError{DisplayErrorCode::DisplayConfigDeviceInfoFailed,
                                static_cast<unsigned long>(sourceResult)};
        }

        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = nativePath.targetInfo.adapterId;
        targetName.header.id = nativePath.targetInfo.id;
        const LONG targetResult = DisplayConfigGetDeviceInfo(&targetName.header);
        if (targetResult != ERROR_SUCCESS) {
            return DisplayError{DisplayErrorCode::DisplayConfigDeviceInfoFailed,
                                static_cast<unsigned long>(targetResult)};
        }

        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO colorInfo{};
        colorInfo.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        colorInfo.header.size = sizeof(colorInfo);
        colorInfo.header.adapterId = nativePath.targetInfo.adapterId;
        colorInfo.header.id = nativePath.targetInfo.id;
        const LONG colorResult = DisplayConfigGetDeviceInfo(&colorInfo.header);
        if (colorResult != ERROR_SUCCESS) {
            return DisplayError{DisplayErrorCode::DisplayConfigDeviceInfoFailed,
                                static_cast<unsigned long>(colorResult)};
        }

        paths.push_back(DisplayPathRecord{
            sourceName.viewGdiDeviceName,
            targetName.monitorFriendlyDeviceName,
            targetName.monitorDevicePath,
            nativePath.targetInfo.adapterId,
            nativePath.targetInfo.id,
            colorInfo.advancedColorEnabled != 0,
        });
    }
    return paths;
}

}  // namespace

TopologyReadResult WindowsDisplayTopologySource::read() {
    MonitorEnumerationContext monitorContext;
    SetLastError(ERROR_SUCCESS);
    if (!EnumDisplayMonitors(
            nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitorContext))) {
        if (monitorContext.error.has_value()) {
            return *monitorContext.error;
        }
        return DisplayError{DisplayErrorCode::MonitorEnumerationFailed, errorOrGenericFailure()};
    }

    auto activePaths = readActivePaths();
    if (const auto* error = std::get_if<DisplayError>(&activePaths)) {
        return *error;
    }
    auto displayPaths = describePaths(std::get<std::vector<DISPLAYCONFIG_PATH_INFO>>(activePaths));
    if (const auto* error = std::get_if<DisplayError>(&displayPaths)) {
        return *error;
    }

    return TopologySnapshot{
        std::move(monitorContext.monitors),
        std::get<std::vector<DisplayPathRecord>>(std::move(displayPaths)),
    };
}

}  // namespace lc::platform::windows
