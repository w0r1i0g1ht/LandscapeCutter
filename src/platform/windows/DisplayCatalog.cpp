#include "platform/windows/DisplayCatalog.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

namespace lc::platform::windows {
namespace {

using IdResult = std::variant<platform::MonitorId, DisplayError>;

IdResult makeMonitorId(const DisplayPathRecord& path) {
    if (path.monitorDevicePath.size() >
        static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return DisplayError{DisplayErrorCode::TopologyValidationFailed, ERROR_INVALID_DATA};
    }

    std::string utf8Path;
    if (!path.monitorDevicePath.empty()) {
        const auto sourceLength = static_cast<int>(path.monitorDevicePath.size());
        const int utf8Length = WideCharToMultiByte(CP_UTF8,
                                                   WC_ERR_INVALID_CHARS,
                                                   path.monitorDevicePath.data(),
                                                   sourceLength,
                                                   nullptr,
                                                   0,
                                                   nullptr,
                                                   nullptr);
        if (utf8Length == 0) {
            return DisplayError{DisplayErrorCode::TopologyValidationFailed, GetLastError()};
        }
        utf8Path.resize(static_cast<std::size_t>(utf8Length));
        if (WideCharToMultiByte(CP_UTF8,
                                WC_ERR_INVALID_CHARS,
                                path.monitorDevicePath.data(),
                                sourceLength,
                                utf8Path.data(),
                                utf8Length,
                                nullptr,
                                nullptr) == 0) {
            return DisplayError{DisplayErrorCode::TopologyValidationFailed, GetLastError()};
        }
    }

    const auto highPart = static_cast<std::uint32_t>(path.adapterId.HighPart);
    const std::uint64_t luidValue =
        (static_cast<std::uint64_t>(highPart) << 32U) | path.adapterId.LowPart;
    std::array<char, 16> luidText{};
    const auto luidConversion =
        std::to_chars(luidText.data(), luidText.data() + luidText.size(), luidValue, 16);
    if (luidConversion.ec != std::errc{}) {
        return DisplayError{DisplayErrorCode::TopologyValidationFailed, ERROR_INVALID_DATA};
    }

    std::array<char, 10> targetText{};
    const auto targetConversion = std::to_chars(
        targetText.data(), targetText.data() + targetText.size(), path.targetId, 10);
    if (targetConversion.ec != std::errc{}) {
        return DisplayError{DisplayErrorCode::TopologyValidationFailed, ERROR_INVALID_DATA};
    }

    std::string value(16U - static_cast<std::size_t>(luidConversion.ptr - luidText.data()), '0');
    value.append(luidText.data(), luidConversion.ptr);
    value.push_back(':');
    value.append(targetText.data(), targetConversion.ptr);
    value.push_back(':');
    value.append(utf8Path);
    return platform::MonitorId{std::move(value)};
}

DisplayError validationError(unsigned long nativeError) {
    return DisplayError{DisplayErrorCode::TopologyValidationFailed, nativeError};
}

}  // namespace

DisplayCatalog::DisplayCatalog(IDisplayTopologySource& source) : source_(source) {}

DisplayCatalog::RefreshResult DisplayCatalog::refresh() {
    auto topologyResult = source_.read();
    if (const auto* error = std::get_if<DisplayError>(&topologyResult)) {
        healthy_ = false;
        return *error;
    }

    const auto& topology = std::get<TopologySnapshot>(topologyResult);
    std::unordered_map<std::wstring, const DisplayPathRecord*> pathsByName;
    pathsByName.reserve(topology.paths.size());
    for (const auto& path : topology.paths) {
        if (!pathsByName.emplace(path.gdiDeviceName, &path).second) {
            healthy_ = false;
            return validationError(ERROR_INVALID_DATA);
        }
    }

    std::vector<MonitorDescriptor> candidate;
    candidate.reserve(topology.monitors.size());
    std::unordered_map<std::wstring, bool> matchedPaths;
    matchedPaths.reserve(topology.monitors.size());
    const std::uint64_t nextGeneration = generation_ + 1;
    for (const auto& nativeMonitor : topology.monitors) {
        const auto path = pathsByName.find(nativeMonitor.gdiDeviceName);
        if (path == pathsByName.end()) {
            healthy_ = false;
            return validationError(ERROR_NOT_FOUND);
        }
        if (!matchedPaths.emplace(nativeMonitor.gdiDeviceName, true).second ||
            nativeMonitor.dpiX == 0 || nativeMonitor.dpiY == 0) {
            healthy_ = false;
            return validationError(ERROR_INVALID_DATA);
        }

        auto idResult = makeMonitorId(*path->second);
        if (const auto* error = std::get_if<DisplayError>(&idResult)) {
            healthy_ = false;
            return *error;
        }
        candidate.push_back(MonitorDescriptor{
            std::get<platform::MonitorId>(std::move(idResult)),
            path->second->friendlyName,
            nativeMonitor.gdiDeviceName,
            nativeMonitor.handle,
            nativeMonitor.desktopRect,
            nativeMonitor.workRect,
            nativeMonitor.dpiX,
            nativeMonitor.dpiY,
            static_cast<double>(nativeMonitor.dpiX) / 96.0,
            nativeMonitor.primary,
            path->second->hdrEnabled,
            path->second->adapterId,
            path->second->targetId,
            nextGeneration,
        });
    }

    if (matchedPaths.size() != pathsByName.size()) {
        healthy_ = false;
        return validationError(ERROR_NOT_FOUND);
    }

    monitors_ = std::move(candidate);
    generation_ = nextGeneration;
    healthy_ = true;
    return monitors_;
}

std::optional<MonitorDescriptor> DisplayCatalog::findByNativeHandle(HMONITOR handle) const {
    const auto found = std::find_if(monitors_.begin(), monitors_.end(), [handle](const auto& monitor) {
        return monitor.nativeHandle == handle;
    });
    if (found == monitors_.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<MonitorDescriptor> DisplayCatalog::monitorContaining(
    platform::PhysicalPoint point) const {
    const auto found = std::find_if(monitors_.begin(), monitors_.end(), [point](const auto& monitor) {
        return platform::contains(monitor.desktopRect, point);
    });
    if (found == monitors_.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<MonitorDescriptor> DisplayCatalog::monitorFromPoint(POINT point) const {
    const HMONITOR handle = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    return findByNativeHandle(handle);
}

std::uint64_t DisplayCatalog::generation() const noexcept {
    return generation_;
}

bool DisplayCatalog::healthy() const noexcept {
    return healthy_;
}

}  // namespace lc::platform::windows
