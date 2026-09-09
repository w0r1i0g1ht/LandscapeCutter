#pragma once
#include "app/CaptureCoordinator.hpp"
#include "snip/SnapshotImage.hpp"
#include <QObject>
#include <functional>

namespace lc::snip {
using ReadbackResult = std::variant<QImage, app::CaptureNoticeCode>;
using ReadbackCompletion = std::function<void(ReadbackResult)>;
using ReadbackFunction = std::function<void(capture::CaptureFrame, ReadbackCompletion)>;
class SnapshotBatch final : public QObject {
    Q_OBJECT
  public:
    SnapshotBatch(capture::IMonitorCaptureService&, graphics::d3d11::ID3d11DeviceRecovery&,
                  ReadbackFunction, QObject* parent = nullptr);
    ~SnapshotBatch() override;
    void start(std::vector<platform::windows::MonitorDescriptor> monitors);
    void cancel();
    bool active() const {
        return active_;
    }
  signals:
    void ready(std::vector<lc::snip::FrozenMonitor> images);
    void failed(lc::app::CaptureNoticeCode error);

  private:
    void next();
    void fail(app::CaptureNoticeCode);
    capture::IMonitorCaptureService& service_;
    graphics::d3d11::ID3d11DeviceRecovery& recovery_;
    ReadbackFunction readback_;
    std::vector<platform::windows::MonitorDescriptor> monitors_;
    std::vector<FrozenMonitor> images_;
    std::uint64_t id_ = 0, deviceGeneration_ = 0;
    bool active_ = false, retried_ = false;
};
} // namespace lc::snip
