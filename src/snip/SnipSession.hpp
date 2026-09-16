#pragma once
#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationInteraction.hpp"
#include "annotation/AnnotationPreparation.hpp"
#include "annotation/AnnotationTypes.hpp"
#include "snip/SelectionModel.hpp"
#include "snip/SnapshotBatch.hpp"
#include "pin/PinTypes.hpp"
#include <QPointer>
#include <QThreadPool>
#include <atomic>
#include <memory>
#include <mutex>

class QFileDialog;
namespace lc::snip {
class SnipOverlay;
using ChooseSavePath = std::function<void(std::function<void(QString)>, std::function<void()>)>;
enum class SnipSessionState {
    Idle,
    PreparingCapture,
    Selecting,
    PreparingAnnotation,
    Annotating,
    PreparingPinFromSelection,
    CreatingPin,
    ChoosingSavePath,
    ExportingFromSelection,
    ExportingAnnotated,
};

class SnipSession final : public QObject {
    Q_OBJECT
  public:
    explicit SnipSession(SnapshotBatch&, QObject* parent = nullptr);
    SnipSession(SnapshotBatch&, PrepareAnnotation, QObject* parent = nullptr);
    SnipSession(SnapshotBatch&, PrepareAnnotation, ChooseSavePath, QObject* parent = nullptr);
    SnipSession(SnapshotBatch&, PrepareAnnotation, ChooseSavePath, pin::CreatePin,
                QObject* parent = nullptr);
    ~SnipSession() override;
    void begin(std::vector<platform::windows::MonitorDescriptor>);
    void cancel();
    void copy();
    void save();
    void pin();
    void beginAnnotation(annotation::AnnotationTool);
    bool active() const {
        return active_;
    }
    [[nodiscard]] SnipSessionState state() const noexcept {
        return state_;
    }
    [[nodiscard]] annotation::AnnotationDocument* document() const noexcept {
        return document_.get();
    }
    [[nodiscard]] annotation::AnnotationInteraction* interaction() const noexcept {
        return interaction_.get();
    }
    [[nodiscard]] QRect lockedSelection() const noexcept {
        return lockedSelection_;
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
    void annotationPrepared(std::uint64_t sessionRequestId, std::uint64_t preparationRequestId,
                            QImage image);
    void preparePinFromSelection();
    void createPin(std::unique_ptr<annotation::AnnotationDocument>, SnipSessionState sourceState,
                   annotation::AnnotationTool tool = annotation::AnnotationTool::Select,
                   annotation::AnnotationStyle style = {}, int mosaicBlockSize = 12);
    void completePinSuccess();
    void invalidateAnnotationPreparation(bool clearSelection = true) noexcept;
    void chooseSavePath();
    void savePathChosen(QString path);
    void savePathCancelled();
    void exportImage(QString path = {}, QByteArray format = {});
    void destroyOverlaysDeferred();
    void setBusy(bool);
    SnapshotBatch& batch_;
    SelectionModel selection_;
    std::vector<FrozenMonitor> images_;
    std::vector<std::unique_ptr<SnipOverlay>> overlays_;
    std::vector<QPointer<QObject>> deferredOverlays_;
    QPointer<QFileDialog> dialog_;
    QThreadPool workers_;
    PrepareAnnotation preparation_;
    ChooseSavePath savePathChooser_;
    pin::CreatePin createPin_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    std::shared_ptr<std::mutex> exportFinalizationMutex_;
    std::unique_ptr<annotation::AnnotationDocument> document_;
    std::unique_ptr<annotation::AnnotationInteraction> interaction_;
    annotation::AnnotationTool tool_{annotation::AnnotationTool::Select};
    std::uint64_t id_ = 0;
    std::uint64_t annotationPreparationId_ = 0;
    QRect lockedSelection_;
    bool active_ = false, busy_ = false, preparing_ = false;
    SnipSessionState state_{SnipSessionState::Idle};
    SnipSessionState saveSourceState_{SnipSessionState::Selecting};
};
} // namespace lc::snip
