#include "snip/SnapshotBatch.hpp"
#include <QPointer>
#include <limits>

namespace lc::snip {
namespace {
app::CaptureNoticeCode notice(capture::CaptureErrorCode code) {
    using E = capture::CaptureErrorCode;
    using N = app::CaptureNoticeCode;
    switch (code) {
    case E::DeviceLost:
        return N::DeviceRecoveryFailed;
    case E::Unsupported:
        return N::Unsupported;
    case E::MonitorUnavailable:
        return N::MonitorUnavailable;
    case E::AccessDenied:
        return N::AccessDenied;
    case E::Timeout:
        return N::Timeout;
    case E::DisplayChanged:
        return N::DisplayChanged;
    case E::DeviceCreationFailed:
        return N::DeviceUnavailable;
    case E::Cancelled:
        return N::CaptureCancelled;
    case E::EmptyFrame:
        return N::InvalidFrame;
    default:
        return N::InternalFailure;
    }
}
} // namespace
SnapshotBatch::SnapshotBatch(capture::IMonitorCaptureService& service,
                             graphics::d3d11::ID3d11DeviceRecovery& recovery,
                             ReadbackFunction readback, QObject* parent)
    : QObject(parent), service_(service), recovery_(recovery), readback_(std::move(readback)) {}
SnapshotBatch::~SnapshotBatch() {
    cancel();
}
void SnapshotBatch::cancel() {
    active_ = false;
    ++id_;
    service_.cancel();
    images_.clear();
    monitors_.clear();
}
void SnapshotBatch::start(std::vector<platform::windows::MonitorDescriptor> monitors) {
    if (active_)
        return;
    monitors_ = std::move(monitors);
    images_.clear();
    retried_ = false;
    active_ = true;
    ++id_;
    deviceGeneration_ = recovery_.generation();
    if (monitors_.empty()) {
        fail(app::CaptureNoticeCode::DisplayUnavailable);
        return;
    }
    for (const auto& m : monitors_) {
        if (!platform::isValid(m.desktopRect) || platform::width(m.desktopRect) > 32768 ||
            platform::height(m.desktopRect) > 32768 ||
            m.catalogGeneration != monitors_[0].catalogGeneration) {
            fail(app::CaptureNoticeCode::InvalidFrame);
            return;
        }
    }
    next();
}
void SnapshotBatch::fail(app::CaptureNoticeCode code) {
    if (code == app::CaptureNoticeCode::DeviceRecoveryFailed && !retried_) {
        retried_ = true;
        ++id_;
        images_.clear();
        service_.cancel();
        if (recovery_.rebuild()) {
            deviceGeneration_ = recovery_.generation();
            next();
            return;
        }
    }
    cancel();
    emit failed(code);
}
void SnapshotBatch::next() {
    if (!active_)
        return;
    if (images_.size() == monitors_.size()) {
        auto images = std::move(images_);
        active_ = false;
        ++id_;
        monitors_.clear();
        emit ready(std::move(images));
        return;
    }
    const auto monitor = monitors_[images_.size()];
    const auto requestId = id_;
    const auto index = images_.size();
    QPointer<SnapshotBatch> self(this);
    service_.captureOnce({monitor}, [self, requestId, index,
                                     monitor](capture::CaptureResult result) {
        if (!self || !self->active_ || self->id_ != requestId || self->images_.size() != index)
            return;
        if (auto* error = std::get_if<capture::CaptureError>(&result)) {
            self->fail(notice(error->code));
            return;
        }
        auto frame = std::get<capture::CaptureFrame>(std::move(result));
        if (frame.sourceMonitor != monitor.id ||
            frame.displayGeneration != monitor.catalogGeneration ||
            frame.deviceGeneration != self->deviceGeneration_ ||
            self->recovery_.generation() != self->deviceGeneration_ ||
            frame.size.width != platform::width(monitor.desktopRect) ||
            frame.size.height != platform::height(monitor.desktopRect)) {
            self->fail(app::CaptureNoticeCode::InvalidFrame);
            return;
        }
        self->readback_(std::move(frame), [self, requestId, index, monitor](ReadbackResult value) {
            if (!self || !self->active_ || self->id_ != requestId || self->images_.size() != index)
                return;
            if (auto* error = std::get_if<app::CaptureNoticeCode>(&value)) {
                self->fail(*error);
                return;
            }
            auto image = std::get<QImage>(std::move(value));
            const QRect geometry(monitor.desktopRect.left, monitor.desktopRect.top,
                                 static_cast<int>(platform::width(monitor.desktopRect)),
                                 static_cast<int>(platform::height(monitor.desktopRect)));
            if (image.isNull() || image.size() != geometry.size() ||
                self->recovery_.generation() != self->deviceGeneration_) {
                self->fail(app::CaptureNoticeCode::InvalidFrame);
                return;
            }
            self->images_.push_back({geometry, std::move(image), monitor.nativeHandle});
            self->next();
        });
    });
}
} // namespace lc::snip
