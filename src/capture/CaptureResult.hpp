#pragma once

#include "capture/CaptureFrame.hpp"
#include <functional>
#include <system_error>
#include <variant>

namespace lc::capture {

enum class CaptureErrorCode {
    Unsupported,
    MonitorUnavailable,
    AccessDenied,
    Timeout,
    EmptyFrame,
    DisplayChanged,
    DeviceLost,
    DeviceCreationFailed,
    Cancelled,
    Internal,
};

struct CaptureError final {
    CaptureErrorCode code;
    std::error_code nativeCode;
};

using CaptureResult = std::variant<CaptureFrame, CaptureError>;
using CaptureCompletion = std::function<void(CaptureResult)>;

}  // namespace lc::capture
