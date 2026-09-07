#include "app/CaptureCoordinator.hpp"

#include <utility>

namespace lc::app {
namespace {
CaptureNoticeCode noticeCode(capture::CaptureErrorCode error) {
    switch (error) {
    case capture::CaptureErrorCode::Unsupported:
        return CaptureNoticeCode::Unsupported;
    case capture::CaptureErrorCode::MonitorUnavailable:
        return CaptureNoticeCode::MonitorUnavailable;
    case capture::CaptureErrorCode::AccessDenied:
        return CaptureNoticeCode::AccessDenied;
    case capture::CaptureErrorCode::Timeout:
        return CaptureNoticeCode::Timeout;
    case capture::CaptureErrorCode::EmptyFrame:
        return CaptureNoticeCode::InvalidFrame;
    case capture::CaptureErrorCode::DisplayChanged:
        return CaptureNoticeCode::DisplayChanged;
    case capture::CaptureErrorCode::DeviceLost:
        return CaptureNoticeCode::DeviceUnavailable;
    case capture::CaptureErrorCode::DeviceCreationFailed:
        return CaptureNoticeCode::DeviceUnavailable;
    case capture::CaptureErrorCode::Cancelled:
        return CaptureNoticeCode::CaptureCancelled;
    case capture::CaptureErrorCode::Internal:
        return CaptureNoticeCode::InternalFailure;
    }
    return CaptureNoticeCode::InternalFailure;
}

CaptureNoticeCode availabilityNotice(CaptureAvailability availability) {
    switch (availability) {
    case CaptureAvailability::Available:
        return CaptureNoticeCode::InternalFailure;
    case CaptureAvailability::Unsupported:
        return CaptureNoticeCode::Unsupported;
    case CaptureAvailability::DisplayUnavailable:
        return CaptureNoticeCode::DisplayUnavailable;
    case CaptureAvailability::DeviceUnavailable:
        return CaptureNoticeCode::DeviceUnavailable;
    }
    return CaptureNoticeCode::InternalFailure;
}
}  // namespace

CaptureCoordinator::CaptureCoordinator(capture::IMonitorCaptureService& captureService,
                                       graphics::d3d11::ID3d11DeviceRecovery& deviceRecovery,
                                       MonitorSelector monitorSelector, QObject* parent)
    : QObject(parent),
      captureService_(captureService),
      deviceRecovery_(deviceRecovery),
      monitorSelector_(std::move(monitorSelector)) {}

CaptureCoordinator::~CaptureCoordinator() { shutdown(); }

void CaptureCoordinator::requestCapture() {
    if (shuttingDown_) { return; }
    if (state_ == CaptureState::Capturing) {
        emit noticeReady(notice(CaptureNoticeCode::Busy));
        return;
    }
    if (availability_ != CaptureAvailability::Available) {
        emit noticeReady(notice(availabilityNotice(availability_)));
        return;
    }

    auto selected = monitorSelector_();
    if (!selected) {
        emit noticeReady(notice(CaptureNoticeCode::MonitorUnavailable));
        return;
    }

    const auto displayGeneration = selected->catalogGeneration;
    active_.emplace(ActiveCapture{
        .monitor = std::move(*selected),
        .started = std::chrono::steady_clock::now(),
        .displayGeneration = displayGeneration,
        .deviceGeneration = deviceRecovery_.generation(),
        .id = ++nextCaptureId_,
    });
    state_ = CaptureState::Capturing;
    startAttempt();
}

void CaptureCoordinator::setAvailability(CaptureAvailability availability) {
    if (shuttingDown_) { return; }
    setAvailabilityInternal(availability);
}

void CaptureCoordinator::shutdown() noexcept {
    if (shuttingDown_) { return; }
    shuttingDown_ = true;
    if (state_ == CaptureState::Capturing) { captureService_.cancel(); }
    active_.reset();
    latestFrame_.reset();
    state_ = CaptureState::Idle;
}

CaptureState CaptureCoordinator::state() const noexcept { return state_; }

const std::optional<capture::CaptureFrame>& CaptureCoordinator::latestFrame() const noexcept {
    return latestFrame_;
}

void CaptureCoordinator::startAttempt() {
    const auto captureId = active_->id;
    const auto attempt = ++active_->attempt;
    capture::MonitorCaptureRequest request{.monitor = active_->monitor};
    QPointer<CaptureCoordinator> coordinator(this);
    captureService_.captureOnce(std::move(request), [coordinator, captureId, attempt](capture::CaptureResult result) {
        if (coordinator) { coordinator->complete(captureId, attempt, std::move(result)); }
    });
}

void CaptureCoordinator::complete(std::uint64_t captureId, std::uint32_t attempt,
                                  capture::CaptureResult result) {
    if (shuttingDown_ || !active_ || active_->id != captureId || active_->attempt != attempt) { return; }

    const auto currentDeviceGeneration = deviceRecovery_.generation();
    if (latestFrame_ && latestFrame_->deviceGeneration != currentDeviceGeneration) {
        latestFrame_.reset();
    }

    const auto currentMonitor = monitorSelector_();
    if (!currentMonitor) {
        finish(CaptureNoticeCode::DisplayUnavailable, CaptureAvailability::DisplayUnavailable);
        return;
    }
    if (currentMonitor->catalogGeneration != active_->displayGeneration) {
        finish(CaptureNoticeCode::DisplayChanged);
        return;
    }

    if (currentDeviceGeneration != active_->deviceGeneration) {
        finish(CaptureNoticeCode::DeviceUnavailable, CaptureAvailability::DeviceUnavailable);
        return;
    }

    if (auto* completedFrame = std::get_if<capture::CaptureFrame>(&result)) {
        if (completedFrame->displayGeneration != active_->displayGeneration) {
            finish(CaptureNoticeCode::DisplayChanged);
            return;
        }
        if (completedFrame->deviceGeneration != active_->deviceGeneration) {
            finish(CaptureNoticeCode::DeviceUnavailable, CaptureAvailability::DeviceUnavailable);
            return;
        }

        latestFrame_.emplace(std::move(*completedFrame));
        finish(CaptureNoticeCode::Success);
        return;
    }

    const auto error = std::get<capture::CaptureError>(result).code;
    if (error == capture::CaptureErrorCode::DeviceLost) {
        latestFrame_.reset();
    }
    if (error == capture::CaptureErrorCode::DeviceLost && active_->retryCount == 0) {
        ++active_->retryCount;
        QPointer<CaptureCoordinator> coordinator(this);
        captureService_.cancel();
        if (!coordinator || shuttingDown_ || !active_ || active_->id != captureId ||
            active_->attempt != attempt) {
            return;
        }
        if (!deviceRecovery_.rebuild()) {
            finish(CaptureNoticeCode::DeviceRecoveryFailed, CaptureAvailability::DeviceUnavailable);
            return;
        }
        if (!coordinator) { return; }
        active_->deviceGeneration = deviceRecovery_.generation();
        startAttempt();
        return;
    }

    const auto code = noticeCode(error);
    const auto availability = error == capture::CaptureErrorCode::Unsupported
                                  ? std::optional{CaptureAvailability::Unsupported}
                              : error == capture::CaptureErrorCode::DeviceLost ||
                                      error == capture::CaptureErrorCode::DeviceCreationFailed
                                  ? std::optional{CaptureAvailability::DeviceUnavailable}
                                  : std::optional<CaptureAvailability>{};
    finish(code, availability);
}

void CaptureCoordinator::finish(CaptureNoticeCode code,
                                std::optional<CaptureAvailability> availability) {
    const auto completed = notice(code);
    active_.reset();
    state_ = CaptureState::Idle;
    if (availability && availability_ != *availability) {
        availability_ = *availability;
        QPointer<CaptureCoordinator> coordinator(this);
        emit availabilityChanged(availability_);
        if (!coordinator) { return; }
    }
    emit noticeReady(completed);
}

void CaptureCoordinator::setAvailabilityInternal(CaptureAvailability availability) {
    if (availability_ == availability) { return; }
    availability_ = availability;
    emit availabilityChanged(availability_);
}

CaptureNotice CaptureCoordinator::notice(CaptureNoticeCode code) const {
    CaptureNotice value{.code = code, .elapsed = std::chrono::milliseconds{0}};
    if (active_) {
        value.monitor = active_->monitor.id;
        value.displayName = active_->monitor.displayName;
        value.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - active_->started);
    }
    if (code == CaptureNoticeCode::Success && latestFrame_) {
        value.size = latestFrame_->size;
        value.pixelFormat = latestFrame_->pixelFormat;
    }
    return value;
}

}  // namespace lc::app
