#pragma once

#include "platform/MonitorTypes.hpp"
#include <d3d11.h>
#include <winrt/base.h>
#include <chrono>
#include <optional>

namespace lc::capture {

enum class CapturePixelFormat { Bgra8Unorm, Rgba16Float };

struct CaptureFrame final {
    CaptureFrame() = default;
    CaptureFrame(CaptureFrame&&) noexcept = default;
    CaptureFrame& operator=(CaptureFrame&&) noexcept = default;
    CaptureFrame(const CaptureFrame&) = delete;
    CaptureFrame& operator=(const CaptureFrame&) = delete;

    winrt::com_ptr<ID3D11Texture2D> texture;
    platform::PixelSize size{};
    CapturePixelFormat pixelFormat{};
    std::optional<std::chrono::nanoseconds> systemRelativeTime;
    platform::MonitorId sourceMonitor;
    std::uint64_t displayGeneration{};
    std::uint64_t deviceGeneration{};
};

}  // namespace lc::capture
