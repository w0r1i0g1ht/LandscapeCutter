#include "snip/SnapshotReadback.hpp"
#include <QMetaObject>
namespace lc::snip {
ReadbackFunction SnapshotReadback::function() {
    return [this](capture::CaptureFrame frame, ReadbackCompletion done) {
        pool_.start([this, frame = std::move(frame), done = std::move(done)]() mutable {
            ReadbackResult result = app::CaptureNoticeCode::InternalFailure;
            try {
                if (frame.texture) {
                    graphics::d3d11::TextureCopy copy(manager_);
                    auto raw = copy.readback(*frame.texture);
                    if (auto* error = std::get_if<graphics::d3d11::D3dError>(&raw)) {
                        result = error->code == graphics::d3d11::D3dErrorCode::DeviceLost
                                     ? app::CaptureNoticeCode::DeviceRecoveryFailed
                                     : app::CaptureNoticeCode::InvalidFrame;
                    } else
                        result = imageFromReadback(std::get<graphics::d3d11::TextureReadback>(raw));
                }
            } catch (...) {
            }
            QMetaObject::invokeMethod(
                this,
                [done = std::move(done), result = std::move(result)]() mutable {
                    done(std::move(result));
                },
                Qt::QueuedConnection);
        });
    };
}
} // namespace lc::snip
