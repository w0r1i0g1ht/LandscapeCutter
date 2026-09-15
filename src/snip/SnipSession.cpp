#include "snip/SnipSession.hpp"
#include "annotation/AnnotationRenderer.hpp"
#include "app/AppController.hpp"
#include "snip/SnipOverlay.hpp"
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>

#include <algorithm>

namespace lc::snip {
SnipSession::SnipSession(SnapshotBatch& batch, QObject* parent) : QObject(parent), batch_(batch) {
    workers_.setMaxThreadCount(1);
    connect(&batch_, &SnapshotBatch::ready, this, &SnipSession::open);
    connect(&batch_, &SnapshotBatch::failed, this, [this](app::CaptureNoticeCode code) {
        if (!active_)
            return;
        cancel();
        emit errorOccurred(app::formatCaptureNotice({.code = code}));
    });
}
SnipSession::SnipSession(SnapshotBatch& batch, PrepareAnnotation preparation, QObject* parent)
    : SnipSession(batch, parent) {
    preparation_ = std::move(preparation);
}
SnipSession::SnipSession(SnapshotBatch& batch, PrepareAnnotation preparation,
                         ChooseSavePath savePathChooser, QObject* parent)
    : SnipSession(batch, std::move(preparation), parent) {
    savePathChooser_ = std::move(savePathChooser);
}
SnipSession::SnipSession(SnapshotBatch& batch, PrepareAnnotation preparation,
                         ChooseSavePath savePathChooser, pin::CreatePin createPin, QObject* parent)
    : SnipSession(batch, std::move(preparation), std::move(savePathChooser), parent) {
    createPin_ = std::move(createPin);
}
SnipSession::~SnipSession() {
    cancel();
    workers_.waitForDone();
    for (const auto& overlay : deferredOverlays_) {
        if (overlay)
            QCoreApplication::sendPostedEvents(overlay, QEvent::DeferredDelete);
    }
    deferredOverlays_.clear();
}
void SnipSession::begin(std::vector<platform::windows::MonitorDescriptor> monitors) {
    if (active_)
        return;
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    exportFinalizationMutex_ = std::make_shared<std::mutex>();
    active_ = true;
    preparing_ = true;
    state_ = SnipSessionState::PreparingCapture;
    ++id_;
    batch_.start(std::move(monitors));
}
void SnipSession::cancel() {
    if (cancellation_) {
        if (exportFinalizationMutex_) {
            const std::lock_guard lock(*exportFinalizationMutex_);
            cancellation_->store(true, std::memory_order_release);
        } else {
            cancellation_->store(true, std::memory_order_release);
        }
    }
    active_ = false;
    preparing_ = false;
    busy_ = false;
    state_ = SnipSessionState::Idle;
    ++id_;
    invalidateAnnotationPreparation();
    batch_.cancel();
    if (dialog_) {
        dialog_->reject();
        dialog_->deleteLater();
        dialog_ = nullptr;
    }
    destroyOverlaysDeferred();
    images_.clear();
    selection_.clear();
    interaction_.reset();
    document_.reset();
}

void SnipSession::destroyOverlaysDeferred() {
    std::erase_if(deferredOverlays_, [](const QPointer<QObject>& overlay) { return overlay.isNull(); });
    // Defer deletion so a button/key event can return to its sender safely.
    for (auto& overlay : overlays_) {
        overlay->clearAnnotationContext();
        overlay->hide();
        auto* deferred = overlay.release();
        deferredOverlays_.emplace_back(deferred);
        deferred->deleteLater();
    }
    overlays_.clear();
}
void SnipSession::open(std::vector<FrozenMonitor> images) {
    if (!active_ || !preparing_)
        return;
    if (images.empty()) {
        cancel();
        return;
    }
    preparing_ = false;
    state_ = SnipSessionState::Selecting;
    images_ = std::move(images);
    QRect bounds;
    for (const auto& image : images_)
        bounds = bounds.united(image.geometry);
    if (bounds.isEmpty() || bounds.width() > 32768 || bounds.height() > 32768) {
        cancel();
        emit errorOccurred(QStringLiteral("桌面范围过大，无法截图。"));
        return;
    }
    selection_.setBounds(bounds);
    for (const auto& image : images_) {
        auto overlay = std::make_unique<SnipOverlay>(image, selection_);
        auto* source = overlay.get();
        connect(source, &SnipOverlay::selectionChanged, this, [this, source] {
            for (auto& window : overlays_)
                window->setToolbarHost(window.get() == source);
        });
        connect(overlay.get(), &SnipOverlay::copyRequested, this, &SnipSession::copy);
        connect(overlay.get(), &SnipOverlay::saveRequested, this, &SnipSession::save);
        connect(overlay.get(), &SnipOverlay::pinRequested, this, &SnipSession::pin);
        connect(overlay.get(), &SnipOverlay::cancelRequested, this, &SnipSession::cancel);
        connect(overlay.get(), &SnipOverlay::annotationToolRequested, this,
                [this](annotation::AnnotationTool tool) {
                    if (state_ == SnipSessionState::Selecting) {
                        beginAnnotation(tool);
                    } else if (state_ == SnipSessionState::Annotating && interaction_) {
                        interaction_->setTool(tool);
                        for (auto& window : overlays_)
                            window->refresh();
                    }
                });
        connect(overlay.get(), &SnipOverlay::annotationChanged, this, [this] {
            for (auto& window : overlays_)
                window->refresh();
        });
        connect(overlay.get(), &SnipOverlay::annotationUndoRequested, this, [this] {
            if (state_ == SnipSessionState::Annotating && document_ && document_->undo()) {
                for (auto& window : overlays_)
                    window->refresh();
            }
        });
        connect(overlay.get(), &SnipOverlay::annotationRedoRequested, this, [this] {
            if (state_ == SnipSessionState::Annotating && document_ && document_->redo()) {
                for (auto& window : overlays_)
                    window->refresh();
            }
        });
        connect(overlay.get(), &SnipOverlay::annotationDeleteRequested, this, [this] {
            if (state_ == SnipSessionState::Annotating && interaction_ && interaction_->deleteSelection()) {
                for (auto& window : overlays_)
                    window->refresh();
            }
        });
        connect(overlay.get(), &SnipOverlay::annotationTextCreateRequested, this,
                [this](QPointF anchor) {
                    if (state_ != SnipSessionState::Annotating)
                        return;
                    const auto host = std::find_if(overlays_.begin(), overlays_.end(),
                                                   [](const auto& window) { return window->isToolbarHost(); });
                    if (host != overlays_.end())
                        (*host)->createTextEditor(anchor);
                });
        connect(overlay.get(), &SnipOverlay::annotationTextEditRequested, this,
                [this](annotation::AnnotationId id) {
                    if (state_ != SnipSessionState::Annotating)
                        return;
                    const auto host = std::find_if(overlays_.begin(), overlays_.end(),
                                                   [](const auto& window) { return window->isToolbarHost(); });
                    if (host != overlays_.end())
                        (*host)->editTextEditor(id);
                });
        connect(overlay.get(), &SnipOverlay::displayInvalidated, this, &SnipSession::cancel,
                Qt::QueuedConnection);
        overlays_.push_back(std::move(overlay));
    }
    for (std::size_t index = 0; index < overlays_.size(); ++index)
        overlays_[index]->setToolbarHost(index == 0);
    for (auto& overlay : overlays_)
        overlay->show();
    if (!overlays_.empty()) {
        overlays_.front()->activateWindow();
        overlays_.front()->setFocus();
    }
}
void SnipSession::setBusy(bool busy) {
    busy_ = busy;
    for (auto& overlay : overlays_)
        overlay->setBusy(busy);
}
void SnipSession::copy() {
    if (active_ && !busy_ && !preparing_ &&
        ((state_ == SnipSessionState::Selecting && !selection_.rect().isEmpty()) ||
         (state_ == SnipSessionState::Annotating && document_)))
        exportImage();
}
void SnipSession::save() {
    if (!active_ || busy_ || preparing_ ||
        !((state_ == SnipSessionState::Selecting && !selection_.rect().isEmpty()) ||
          (state_ == SnipSessionState::Annotating && document_)))
        return;
    saveSourceState_ = state_;
    setBusy(true);
    state_ = SnipSessionState::ChoosingSavePath;
    if (savePathChooser_) {
        const auto requestId = id_;
        const QPointer<SnipSession> session(this);
        savePathChooser_(
            [session, requestId](QString path) {
                if (session && requestId == session->id_)
                    session->savePathChosen(std::move(path));
            },
            [session, requestId] {
                if (session && requestId == session->id_)
                    session->savePathCancelled();
            });
        return;
    }
    chooseSavePath();
}

void SnipSession::pin() {
    if (!active_ || busy_ || !createPin_)
        return;
    if (state_ == SnipSessionState::Selecting && !selection_.rect().isEmpty()) {
        preparePinFromSelection();
        return;
    }
    if (state_ != SnipSessionState::Annotating || !document_ || !interaction_)
        return;
    const auto tool = interaction_->tool();
    const auto style = interaction_->style();
    const auto blockSize = interaction_->mosaicBlockSize();
    setBusy(true);
    interaction_->cancelDraft();
    document_->clearSelection();
    for (auto& overlay : overlays_)
        overlay->clearAnnotationContext();
    interaction_.reset();
    state_ = SnipSessionState::CreatingPin;
    createPin(std::move(document_), SnipSessionState::Annotating, tool, style, blockSize);
}

void SnipSession::preparePinFromSelection() {
    const auto sessionRequestId = id_;
    const auto preparationRequestId = ++annotationPreparationId_;
    lockedSelection_ = selection_.rect();
    const auto images = images_;
    const auto cancellation = cancellation_;
    state_ = SnipSessionState::PreparingPinFromSelection;
    setBusy(true);
    const QPointer<SnipSession> session(this);
    const auto complete = [session, sessionRequestId, preparationRequestId](QImage image) {
        if (!session)
            return;
        QMetaObject::invokeMethod(session, [session, sessionRequestId, preparationRequestId,
                                             image = std::move(image)]() mutable {
            if (session)
                session->annotationPrepared(sessionRequestId, preparationRequestId, std::move(image));
        }, Qt::QueuedConnection);
    };
    if (preparation_) {
        try { preparation_(images, lockedSelection_, cancellation, complete); } catch (...) { complete({}); }
    } else {
        workers_.start([images, selection = lockedSelection_, cancellation, complete] {
            try { prepareAnnotation(images, selection, cancellation, complete); } catch (...) { complete({}); }
        });
    }
}

void SnipSession::createPin(std::unique_ptr<annotation::AnnotationDocument> document,
                            const SnipSessionState sourceState, const annotation::AnnotationTool tool,
                            const annotation::AnnotationStyle style, const int mosaicBlockSize) {
    if (!active_ || !document)
        return;
    pin::PinCreateResult result;
    try {
        result = createPin_(document, lockedSelection_.topLeft());
    } catch (...) {
        result.rejectedDocument = std::move(document);
        result.error = QStringLiteral("创建贴图失败。");
    }
    if (result.id.has_value()) {
        completePinSuccess();
        return;
    }
    if (!result.rejectedDocument)
        result.rejectedDocument = std::move(document);
    const QString error = result.error.isEmpty() ? QStringLiteral("创建贴图失败。") : result.error;
    if (sourceState == SnipSessionState::Selecting) {
        interaction_.reset();
        document_.reset();
        state_ = SnipSessionState::Selecting;
        setBusy(false);
        for (auto& overlay : overlays_)
            overlay->clearAnnotationContext();
        emit errorOccurred(error);
        return;
    }
    document_ = std::move(result.rejectedDocument);
    if (!document_) {
        emit errorOccurred(error);
        cancel();
        return;
    }
    interaction_ = std::make_unique<annotation::AnnotationInteraction>(*document_);
    interaction_->setTool(tool);
    interaction_->setStyle(style);
    interaction_->setMosaicBlockSize(mosaicBlockSize);
    state_ = sourceState;
    setBusy(false);
    for (auto& overlay : overlays_)
        overlay->setAnnotationContext(document_.get(), interaction_.get(), lockedSelection_);
    emit errorOccurred(error);
}

void SnipSession::completePinSuccess() {
    active_ = false;
    busy_ = false;
    preparing_ = false;
    state_ = SnipSessionState::Idle;
    ++id_;
    invalidateAnnotationPreparation();
    destroyOverlaysDeferred();
    images_.clear();
    selection_.clear();
    interaction_.reset();
    document_.reset();
}

void SnipSession::chooseSavePath() {
    auto* dialog = new QFileDialog(overlays_.front().get(), QStringLiteral("保存截图"));
    dialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    // Non-native dialog lets suffix normalization happen before its overwrite confirmation.
    dialog->setOption(QFileDialog::DontUseNativeDialog);
    dialog->setNameFilters({QStringLiteral("PNG (*.png)"), QStringLiteral("JPEG (*.jpg *.jpeg)")});
    dialog->setDefaultSuffix(QStringLiteral("png"));
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    dialog->selectFile(QStringLiteral("LandscapeCutter-%1.png")
                           .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
    connect(dialog, &QFileDialog::filterSelected, dialog, [dialog](const QString& filter) {
        const QString suffix =
            filter.startsWith("JPEG") ? QStringLiteral("jpg") : QStringLiteral("png");
        dialog->setDefaultSuffix(suffix);
        const auto files = dialog->selectedFiles();
        if (!files.isEmpty())
            dialog->selectFile(QFileInfo(files.front()).completeBaseName() + '.' + suffix);
    });
    connect(dialog, &QFileDialog::rejected, this, &SnipSession::savePathCancelled);
    connect(dialog, &QFileDialog::accepted, this, [this, dialog] {
        dialog_ = nullptr;
        const auto files = dialog->selectedFiles();
        if (files.isEmpty()) {
            savePathCancelled();
            return;
        }
        savePathChosen(files.front());
    });
    dialog->open();
}

void SnipSession::savePathChosen(QString path) {
    if (!active_ || state_ != SnipSessionState::ChoosingSavePath)
        return;
    const auto suffix = QFileInfo(path).suffix().toLower();
    QByteArray format;
    if (suffix == "png")
        format = "png";
    else if (suffix == "jpg" || suffix == "jpeg")
        format = "jpeg";
    else {
        state_ = saveSourceState_;
        setBusy(false);
        emit errorOccurred(QStringLiteral("请使用 .png、.jpg 或 .jpeg 文件扩展名。"));
        return;
    }
    exportImage(std::move(path), std::move(format));
}

void SnipSession::savePathCancelled() {
    dialog_ = nullptr;
    if (active_ && state_ == SnipSessionState::ChoosingSavePath) {
        state_ = saveSourceState_;
        setBusy(false);
    }
}
void SnipSession::exportImage(QString path, QByteArray format) {
    const bool annotated = state_ == SnipSessionState::Annotating ||
                           (state_ == SnipSessionState::ChoosingSavePath &&
                            saveSourceState_ == SnipSessionState::Annotating);
    if (!active_ || (annotated && !document_))
        return;
    const auto sourceState = annotated ? SnipSessionState::Annotating : SnipSessionState::Selecting;
    annotation::AnnotationSnapshot annotationSnapshot;
    if (annotated)
        annotationSnapshot = document_->snapshot();
    setBusy(true);
    state_ = annotated ? SnipSessionState::ExportingAnnotated : SnipSessionState::ExportingFromSelection;
    const auto requestId = id_;
    const auto rect = selection_.rect();
    const auto images = images_;
    const auto cancellation = cancellation_;
    const auto finalizationMutex = exportFinalizationMutex_;
    const QPointer<SnipSession> session(this);
    workers_.start([session, requestId, rect, images, annotationSnapshot = std::move(annotationSnapshot),
                    annotated, sourceState, path = std::move(path), format = std::move(format), cancellation,
                    finalizationMutex] {
        QImage result;
        QString error;
        try {
            result = annotated ? annotation::composeAnnotations(annotationSnapshot)
                               : composeSelection(images, rect);
            if (result.isNull())
                error = QStringLiteral("选区没有有效画面，或图像过大。");
            else if (!path.isEmpty())
                error = saveImage(result, path, format, cancellation.get(), finalizationMutex.get());
        } catch (...) {
            error = QStringLiteral("图像处理失败，请缩小选区后重试。");
        }
        QMetaObject::invokeMethod(
            session,
            [session, requestId, sourceState, result = std::move(result), error, path] {
                if (!session || !session->active_ || requestId != session->id_)
                    return;
                if (!error.isEmpty()) {
                    session->state_ = sourceState;
                    session->setBusy(false);
                    emit session->errorOccurred(error);
                    return;
                }
                if (path.isEmpty())
                    QApplication::clipboard()->setImage(result);
                session->cancel();
            },
            Qt::QueuedConnection);
    });
}

void SnipSession::beginAnnotation(annotation::AnnotationTool tool) {
    if (!active_ || state_ != SnipSessionState::Selecting || busy_ || selection_.rect().isEmpty())
        return;

    tool_ = tool;
    const auto sessionRequestId = id_;
    const auto preparationRequestId = ++annotationPreparationId_;
    lockedSelection_ = selection_.rect();
    const auto images = images_;
    const auto cancellation = cancellation_;
    state_ = SnipSessionState::PreparingAnnotation;
    setBusy(true);
    const QPointer<SnipSession> session(this);
    const auto complete = [session, sessionRequestId, preparationRequestId](QImage image) {
        if (!session)
            return;
        QMetaObject::invokeMethod(session,
                                  [session, sessionRequestId, preparationRequestId,
                                   image = std::move(image)]() mutable {
            if (session)
                session->annotationPrepared(sessionRequestId, preparationRequestId, std::move(image));
        }, Qt::QueuedConnection);
    };
    if (preparation_) {
        try {
            preparation_(images, lockedSelection_, cancellation, complete);
        } catch (...) {
            complete({});
        }
        return;
    }
    workers_.start([images, lockedSelection = lockedSelection_, cancellation, complete] {
        try {
            prepareAnnotation(images, lockedSelection, cancellation, complete);
        } catch (...) {
            complete({});
        }
    });
}

void SnipSession::annotationPrepared(std::uint64_t sessionRequestId,
                                     std::uint64_t preparationRequestId, QImage image) {
    if (!active_ || sessionRequestId != id_ || preparationRequestId != annotationPreparationId_ ||
        (state_ != SnipSessionState::PreparingAnnotation && state_ != SnipSessionState::PreparingPinFromSelection) ||
        !cancellation_ || cancellation_->load(std::memory_order_acquire))
        return;
    if (image.isNull()) {
        invalidateAnnotationPreparation();
        state_ = SnipSessionState::Selecting;
        setBusy(false);
        emit errorOccurred(QStringLiteral("选区没有有效画面，或图像过大。"));
        return;
    }
    document_ = std::make_unique<annotation::AnnotationDocument>(std::move(image));
    if (state_ == SnipSessionState::PreparingPinFromSelection) {
        invalidateAnnotationPreparation(false);
        selection_.release();
        state_ = SnipSessionState::CreatingPin;
        createPin(std::move(document_), SnipSessionState::Selecting);
        return;
    }
    interaction_ = std::make_unique<annotation::AnnotationInteraction>(*document_);
    interaction_->setTool(tool_);
    invalidateAnnotationPreparation(false);
    selection_.release();
    state_ = SnipSessionState::Annotating;
    setBusy(false);
    for (auto& overlay : overlays_)
        overlay->setAnnotationContext(document_.get(), interaction_.get(), lockedSelection_);
}

void SnipSession::invalidateAnnotationPreparation(bool clearSelection) noexcept {
    ++annotationPreparationId_;
    if (clearSelection)
        lockedSelection_ = {};
}
} // namespace lc::snip
