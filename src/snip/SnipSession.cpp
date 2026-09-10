#include "snip/SnipSession.hpp"
#include "app/AppController.hpp"
#include "snip/SnipOverlay.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>

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
SnipSession::~SnipSession() {
    cancel();
    workers_.waitForDone();
}
void SnipSession::begin(std::vector<platform::windows::MonitorDescriptor> monitors) {
    if (active_)
        return;
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    active_ = true;
    preparing_ = true;
    state_ = SnipSessionState::PreparingCapture;
    ++id_;
    batch_.start(std::move(monitors));
}
void SnipSession::cancel() {
    if (cancellation_)
        cancellation_->store(true, std::memory_order_release);
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
    // Defer deletion so a button/key event can return to its sender safely.
    for (auto& overlay : overlays_) {
        overlay->hide();
        overlay.release()->deleteLater();
    }
    overlays_.clear();
    images_.clear();
    selection_.clear();
    document_.reset();
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
        connect(overlay.get(), &SnipOverlay::cancelRequested, this, &SnipSession::cancel);
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
    if (active_ && state_ == SnipSessionState::Selecting && !busy_ && !preparing_ &&
        !selection_.rect().isEmpty())
        exportImage();
}
void SnipSession::save() {
    if (!active_ || state_ != SnipSessionState::Selecting || busy_ || preparing_ ||
        selection_.rect().isEmpty())
        return;
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
        state_ = SnipSessionState::Selecting;
        setBusy(false);
        emit errorOccurred(QStringLiteral("请使用 .png、.jpg 或 .jpeg 文件扩展名。"));
        return;
    }
    exportImage(std::move(path), std::move(format));
}

void SnipSession::savePathCancelled() {
    dialog_ = nullptr;
    if (active_ && state_ == SnipSessionState::ChoosingSavePath) {
        state_ = SnipSessionState::Selecting;
        setBusy(false);
    }
}
void SnipSession::exportImage(QString path, QByteArray format) {
    setBusy(true);
    state_ = SnipSessionState::ExportingFromSelection;
    const auto requestId = id_;
    const auto rect = selection_.rect();
    const auto images = images_;
    const auto cancellation = cancellation_;
    workers_.start([this, requestId, rect, images, path = std::move(path),
                    format = std::move(format), cancellation] {
        QImage result;
        QString error;
        try {
            result = composeSelection(images, rect);
            if (result.isNull())
                error = QStringLiteral("选区没有有效画面，或图像过大。");
            else if (!path.isEmpty())
                error = saveImage(result, path, format, cancellation.get());
        } catch (...) {
            error = QStringLiteral("图像处理失败，请缩小选区后重试。");
        }
        QMetaObject::invokeMethod(
            this,
            [this, requestId, result = std::move(result), error, path] {
                if (!active_ || requestId != id_)
                    return;
                if (!error.isEmpty()) {
                    state_ = SnipSessionState::Selecting;
                    setBusy(false);
                    emit errorOccurred(error);
                    return;
                }
                if (path.isEmpty())
                    QApplication::clipboard()->setImage(result);
                cancel();
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
        state_ != SnipSessionState::PreparingAnnotation ||
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
    invalidateAnnotationPreparation(false);
    selection_.release();
    state_ = SnipSessionState::Annotating;
    setBusy(false);
    for (auto& overlay : overlays_)
        overlay->refresh();
}

void SnipSession::invalidateAnnotationPreparation(bool clearSelection) noexcept {
    ++annotationPreparationId_;
    if (clearSelection)
        lockedSelection_ = {};
}
} // namespace lc::snip
