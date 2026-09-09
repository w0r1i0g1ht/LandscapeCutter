#pragma once
#include "snip/SelectionModel.hpp"
#include "snip/SnapshotBatch.hpp"
#include <QPointer>
#include <QThreadPool>
#include <atomic>
#include <memory>

class QFileDialog;
namespace lc::snip {
class SnipOverlay;
class SnipSession final : public QObject {
    Q_OBJECT
  public:
    explicit SnipSession(SnapshotBatch&, QObject* parent = nullptr);
    ~SnipSession() override;
    void begin(std::vector<platform::windows::MonitorDescriptor>);
    void cancel();
    void copy();
    void save();
    bool active() const {
        return active_;
    }
    std::size_t overlayCount() const {
        return overlays_.size();
    }
    SelectionModel& selection() {
        return selection_;
    }
  signals:
    void errorOccurred(QString message);

  private:
    void open(std::vector<FrozenMonitor>);
    void exportImage(QString path = {}, QByteArray format = {});
    void setBusy(bool);
    SnapshotBatch& batch_;
    SelectionModel selection_;
    std::vector<FrozenMonitor> images_;
    std::vector<std::unique_ptr<SnipOverlay>> overlays_;
    QPointer<QFileDialog> dialog_;
    QThreadPool workers_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    std::uint64_t id_ = 0;
    bool active_ = false, busy_ = false, preparing_ = false;
};
} // namespace lc::snip
