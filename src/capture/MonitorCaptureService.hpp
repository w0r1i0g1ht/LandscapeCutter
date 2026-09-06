#pragma once

#include "capture/CaptureResult.hpp"
#include "platform/windows/MonitorDescriptor.hpp"

namespace lc::capture {

struct MonitorCaptureRequest final {
    platform::windows::MonitorDescriptor monitor;
    std::chrono::milliseconds timeout{2000};
};

class IMonitorCaptureService {
public:
    virtual ~IMonitorCaptureService() = default;
    virtual void captureOnce(MonitorCaptureRequest request, CaptureCompletion completion) = 0;
    virtual void cancel() noexcept = 0;
};

}  // namespace lc::capture
