#include "pin/PinWindow.hpp"

#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationInteraction.hpp"
#include "annotation/AnnotationRenderer.hpp"
#include "annotation/AnnotationToolbar.hpp"
#include "snip/SnapshotImage.hpp"

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QRunnable>
#include <QWheelEvent>

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>

namespace lc::pin {
struct PinWindow::ExportState {
    std::atomic_bool cancelled{false};
    std::mutex finalizationMutex;
};

PinWindow::PinWindow(const PinId id, ChoosePinSavePath chooser, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), id_(id),
      chooseSavePath_(std::move(chooser)) {
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_QuitOnClose, false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    exportPool_.setMaxThreadCount(1);

    toolbar_ = new annotation::AnnotationToolbar(this);
    toolbar_->setObjectName(QStringLiteral("pinToolbar"));
    toolbar_->setMode(annotation::AnnotationToolbarMode::Pin);
    toolbar_->hide();
    editAction_ = new QAction(tr("编辑"), this);
    editAction_->setObjectName(QStringLiteral("editPinAction"));
    copyAction_ = new QAction(tr("复制"), this);
    copyAction_->setObjectName(QStringLiteral("pinCopyAction"));
    saveAction_ = new QAction(tr("另存为…"), this);
    saveAction_->setObjectName(QStringLiteral("pinSaveAction"));
    resetSizeAction_ = new QAction(tr("恢复原始大小"), this);
    resetSizeAction_->setObjectName(QStringLiteral("resetPinSizeAction"));
    resetOpacityAction_ = new QAction(tr("恢复不透明"), this);
    resetOpacityAction_->setObjectName(QStringLiteral("resetPinOpacityAction"));
    closeAction_ = new QAction(tr("关闭"), this);
    closeAction_->setObjectName(QStringLiteral("closePinAction"));
    connect(editAction_, &QAction::triggered, this, &PinWindow::enterEditing);
    connect(copyAction_, &QAction::triggered, this, &PinWindow::requestCopy);
    connect(saveAction_, &QAction::triggered, this, &PinWindow::requestSave);
    connect(resetSizeAction_, &QAction::triggered, this, &PinWindow::resetSize);
    connect(resetOpacityAction_, &QAction::triggered, this, &PinWindow::resetOpacity);
    connect(closeAction_, &QAction::triggered, this, &QWidget::close);
    connect(toolbar_, &annotation::AnnotationToolbar::toolRequested, this,
            [this](annotation::AnnotationTool tool) {
                if (!interaction_)
                    return;
                if (textEditor_ && tool != interaction_->tool())
                    commitTextEditor();
                interaction_->setTool(tool);
                refresh();
            });
    connect(toolbar_, &annotation::AnnotationToolbar::annotationChanged, this,
            &PinWindow::refresh);
    connect(toolbar_, &annotation::AnnotationToolbar::undoRequested, this, [this] {
        if (document_ && document_->undo())
            refresh();
    });
    connect(toolbar_, &annotation::AnnotationToolbar::redoRequested, this, [this] {
        if (document_ && document_->redo())
            refresh();
    });
    connect(toolbar_, &annotation::AnnotationToolbar::deleteRequested, this, [this] {
        if (interaction_ && interaction_->deleteSelection())
            refresh();
    });
    connect(toolbar_, &annotation::AnnotationToolbar::copyRequested, this,
            &PinWindow::requestCopy);
    connect(toolbar_, &annotation::AnnotationToolbar::saveRequested, this,
            &PinWindow::requestSave);
    connect(toolbar_, &annotation::AnnotationToolbar::doneRequested, this,
            &PinWindow::finishEditing);

    contextMenu_ = new QMenu(this);
    contextMenu_->setObjectName(QStringLiteral("pinContextMenu"));
    contextMenu_->addAction(editAction_);
    contextMenu_->addAction(copyAction_);
    contextMenu_->addAction(saveAction_);
    contextMenu_->addAction(resetSizeAction_);
    contextMenu_->addAction(resetOpacityAction_);
    contextMenu_->addSeparator();
    contextMenu_->addAction(closeAction_);
}

PinWindow::~PinWindow() {
    cancelExportAndWait();
}

QString PinWindow::attachDocument(std::unique_ptr<annotation::AnnotationDocument>& document,
                                  const QPoint preferredTopLeft) {
    if (document_)
        return tr("贴图窗口已经包含图片。");
    if (!document || document->snapshot().base.isNull())
        return tr("无法创建贴图：图片为空。");

    const auto size = document->snapshot().base.size();
    PinGeometryModel geometry(size, QRect(preferredTopLeft, size));
    if (!geometry.valid())
        return tr("无法创建贴图：图片尺寸无效。");
    auto interaction = std::make_unique<annotation::AnnotationInteraction>(*document);

    geometry_ = geometry;
    interaction_ = std::move(interaction);
    document_ = std::move(document);
    toolbar_->setContentAvailable(true);
    toolbar_->setContext(document_.get(), interaction_.get());
    applyGeometry();
    show();
    raise();
    activateWindow();
    refresh();
    return {};
}

annotation::AnnotationDocument* PinWindow::document() const noexcept {
    return document_.get();
}

PinWindowMode PinWindow::mode() const noexcept {
    return mode_;
}

PinId PinWindow::id() const noexcept {
    return id_;
}

void PinWindow::enterEditing() {
    if (!document_ || mode_ != PinWindowMode::Viewing)
        return;
    mode_ = PinWindowMode::Editing;
    dragging_ = false;
    toolbar_->setContext(document_.get(), interaction_.get());
    toolbar_->show();
    positionToolbar();
    refresh();
}

void PinWindow::finishEditing() {
    if (!document_ || mode_ != PinWindowMode::Editing)
        return;
    if (textEditor_)
        commitTextEditor();
    interaction_->cancelDraft();
    document_->clearSelection();
    mode_ = PinWindowMode::Viewing;
    toolbar_->hide();
    refresh();
}

void PinWindow::closeEvent(QCloseEvent* event) {
    cancelExportAndWait();
    cancelTextEditor();
    if (!closeEmitted_) {
        closeEmitted_ = true;
        emit closed(id_);
    }
    QWidget::closeEvent(event);
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event) {
    if (mode_ != PinWindowMode::Viewing) {
        event->ignore();
        return;
    }
    editAction_->setEnabled(document_ != nullptr);
    copyAction_->setEnabled(document_ != nullptr);
    saveAction_->setEnabled(document_ != nullptr);
    resetSizeAction_->setEnabled(geometry_.has_value());
    resetOpacityAction_->setEnabled(geometry_.has_value());
    contextMenu_->popup(event->globalPos());
    event->accept();
}

bool PinWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == textEditor_ && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            key->modifiers().testFlag(Qt::ControlModifier)) {
            commitTextEditor();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            cancelTextEditor();
            refresh();
            return true;
        }
        if ((key->key() == Qt::Key_C || key->key() == Qt::Key_S) &&
            key->modifiers().testFlag(Qt::ControlModifier))
            return true;
    }
    return QWidget::eventFilter(watched, event);
}

void PinWindow::keyPressEvent(QKeyEvent* event) {
    if (mode_ != PinWindowMode::Editing || !document_ || !interaction_) {
        QWidget::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (textEditor_) {
            cancelTextEditor();
            refresh();
        } else if (interaction_->hasDraft()) {
            interaction_->cancelDraft();
            refresh();
        } else {
            finishEditing();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete && interaction_->deleteSelection()) {
        refresh();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Z && event->modifiers().testFlag(Qt::ControlModifier)) {
        const bool redo = event->modifiers().testFlag(Qt::ShiftModifier);
        if (redo ? document_->redo() : document_->undo())
            refresh();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Y && event->modifiers().testFlag(Qt::ControlModifier)) {
        if (document_->redo())
            refresh();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PinWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !document_) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (mode_ == PinWindowMode::Editing) {
        const auto point = documentPoint(event);
        if (interaction_->tool() == annotation::AnnotationTool::Text) {
            editTextAt(point);
        } else if (!textEditor_) {
            interaction_->press(point);
        }
        refresh();
    } else if (mode_ == PinWindowMode::Viewing && geometry_) {
        dragging_ = true;
        dragOffset_ = event->globalPosition().toPoint() - geometry_->windowRect().topLeft();
        grabMouse();
    }
    event->accept();
}

void PinWindow::mouseMoveEvent(QMouseEvent* event) {
    if (mode_ == PinWindowMode::Editing && interaction_ &&
        event->buttons().testFlag(Qt::LeftButton)) {
        interaction_->move(documentPoint(event));
        refresh();
        event->accept();
        return;
    }
    if (mode_ == PinWindowMode::Viewing && dragging_ && geometry_) {
        geometry_->moveTo(event->globalPosition().toPoint() - dragOffset_);
        applyGeometry();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && mode_ == PinWindowMode::Editing && interaction_) {
        interaction_->release(documentPoint(event));
        refresh();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        releaseMouse();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (mode_ == PinWindowMode::Viewing)
            enterEditing();
        else if (mode_ == PinWindowMode::Editing) {
            const auto point = documentPoint(event);
            const auto hit = interaction_ ? interaction_->hitTest(point) : std::nullopt;
            if (!hit.has_value()) {
                finishEditing();
            } else {
                const auto found = std::find_if(document_->objects().begin(), document_->objects().end(),
                                                [hit](const auto& object) { return object.id == *hit; });
                if (found != document_->objects().end() &&
                    std::holds_alternative<annotation::TextAnnotation>(found->payload))
                    editTextAt(point);
            }
        }
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PinWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    if (!document_) {
        painter.fillRect(rect(), Qt::black);
        return;
    }
    auto snapshot = document_->snapshot();
    painter.drawImage(rect(), snapshot.base);
    if (mode_ == PinWindowMode::Editing) {
        if (const auto draft = interaction_->draft(); draft.has_value()) {
            const auto found = std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
                                            [draft](const auto& object) {
                                                return draft->id != 0 && object.id == draft->id;
                                            });
            if (found == snapshot.objects.end())
                snapshot.objects.push_back(*draft);
            else
                *found = *draft;
        }
    }
    annotation::drawAnnotations(painter, snapshot, documentToWindowTransform(), rect());
}

void PinWindow::wheelEvent(QWheelEvent* event) {
    if (mode_ != PinWindowMode::Viewing || !geometry_) {
        QWidget::wheelEvent(event);
        return;
    }
    if (event->modifiers().testFlag(Qt::ControlModifier))
        geometry_->adjustOpacity(event->angleDelta().y());
    else
        geometry_->zoomAt(event->globalPosition().toPoint(), event->angleDelta().y());
    applyGeometry();
    event->accept();
}

void PinWindow::beginTextEditor(QPointF anchor,
                                std::optional<annotation::AnnotationObject> original) {
    if (!document_ || textEditor_)
        return;
    textEditBefore_ = std::move(original);
    textEditAnchor_ = anchor;
    textEditStyle_ = textEditBefore_.has_value()
                         ? std::get<annotation::TextAnnotation>(textEditBefore_->payload).style
                         : annotation::AnnotationStyle{interaction_->style().color, 24.0};
    auto* editor = new QPlainTextEdit(this);
    textEditor_ = editor;
    editor->setObjectName(QStringLiteral("annotationTextEditor"));
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFont(annotation::resolvedAnnotationFont(qMax(1, qRound(textEditStyle_.physicalSize))));
    editor->setPlainText(textEditBefore_.has_value()
                             ? std::get<annotation::TextAnnotation>(textEditBefore_->payload).text
                             : QString{});
    editor->installEventFilter(this);
    const QPointF localAnchor = documentToWindowTransform().map(anchor);
    editor->move(qRound(localAnchor.x()), qRound(localAnchor.y()));
    editor->resize(240, qMax(40, editor->fontMetrics().lineSpacing() * 2));
    editor->show();
    editor->setFocus(Qt::OtherFocusReason);
    editor->raise();
}

void PinWindow::commitTextEditor() {
    if (!textEditor_ || !document_)
        return;
    const QString text = textEditor_->toPlainText();
    if (textEditBefore_.has_value()) {
        auto replacement = *textEditBefore_;
        std::get<annotation::TextAnnotation>(replacement.payload).text = text;
        static_cast<void>(document_->replaceObject(std::move(replacement)));
    } else if (!text.trimmed().isEmpty()) {
        static_cast<void>(document_->addObject(
            annotation::TextAnnotation{textEditAnchor_, text, textEditStyle_}));
    }
    cancelTextEditor();
    refresh();
}

void PinWindow::cancelTextEditor() {
    if (textEditor_) {
        auto* editor = textEditor_.data();
        textEditor_.clear();
        editor->removeEventFilter(this);
        delete editor;
    }
    textEditBefore_.reset();
}

void PinWindow::editTextAt(QPointF point) {
    if (!document_ || !interaction_)
        return;
    const auto hit = interaction_->hitTest(point);
    if (hit.has_value()) {
        const auto found = std::find_if(document_->objects().begin(), document_->objects().end(),
                                        [hit](const auto& object) { return object.id == *hit; });
        if (found != document_->objects().end() &&
            std::holds_alternative<annotation::TextAnnotation>(found->payload)) {
            if (textEditor_)
                return;
            interaction_->cancelDraft();
            beginTextEditor(std::get<annotation::TextAnnotation>(found->payload).anchor, *found);
            return;
        }
    }
    if (!textEditor_)
        beginTextEditor(point);
}

void PinWindow::resetSize() {
    if (!geometry_)
        return;
    geometry_->resetSize();
    applyGeometry();
}

void PinWindow::resetOpacity() {
    if (!geometry_)
        return;
    while (geometry_->opacity() < 1.0)
        geometry_->adjustOpacity(120);
    applyGeometry();
}

void PinWindow::requestCopy() {
    if (!document_ || mode_ == PinWindowMode::ChoosingSavePath || mode_ == PinWindowMode::Exporting)
        return;
    if (textEditor_)
        commitTextEditor();
    const auto snapshot = document_->snapshot();
    const auto priorMode = mode_;
    emit copyRequested(id_);
    beginCopyExport(snapshot, priorMode);
}

void PinWindow::requestSave() {
    if (!document_ || mode_ == PinWindowMode::ChoosingSavePath || mode_ == PinWindowMode::Exporting)
        return;
    if (textEditor_)
        commitTextEditor();
    if (!chooseSavePath_)
        return;
    const auto snapshot = document_->snapshot();
    const auto priorMode = mode_;
    const auto requestId = ++nextRequestId_;
    activeRequestId_ = requestId;
    outputPriorMode_ = priorMode;
    exportState_ = std::make_shared<ExportState>();
    mode_ = PinWindowMode::ChoosingSavePath;
    toolbar_->setBusy(true);
    emit saveRequested(id_);
    QPointer<PinWindow> self(this);
    chooseSavePath_(
        [self, snapshot, priorMode, requestId](QString path) mutable {
            if (!self || self->activeRequestId_ != requestId ||
                self->mode_ != PinWindowMode::ChoosingSavePath || !self->exportState_ ||
                self->exportState_->cancelled.load(std::memory_order_acquire))
                return;
            if (path.isEmpty()) {
                self->restoreOutputMode(requestId);
                return;
            }
            self->beginSaveExport(std::move(path), std::move(snapshot), priorMode, requestId);
        },
        [self, requestId] {
            if (self)
                self->restoreOutputMode(requestId);
        });
}

void PinWindow::beginCopyExport(annotation::AnnotationSnapshot snapshot, const PinWindowMode priorMode) {
    const auto requestId = ++nextRequestId_;
    activeRequestId_ = requestId;
    outputPriorMode_ = priorMode;
    exportState_ = std::make_shared<ExportState>();
    const auto state = exportState_;
    mode_ = PinWindowMode::Exporting;
    toolbar_->setBusy(true);
    QPointer<PinWindow> self(this);
    exportPool_.start(QRunnable::create([self, snapshot = std::move(snapshot), state, requestId] {
        if (state->cancelled.load(std::memory_order_acquire))
            return;
        QImage image;
        QString error;
        try {
            image = annotation::composeAnnotations(snapshot);
            if (image.isNull())
                error = QStringLiteral("无法生成贴图图像。");
        } catch (const std::exception& exception) {
            error = QString::fromUtf8(exception.what());
        } catch (...) {
            error = QStringLiteral("生成贴图图像时发生未知错误。");
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, state, requestId,
                                                                   image = std::move(image), error = std::move(error)]() mutable {
            if (!self || state->cancelled.load(std::memory_order_acquire) ||
                self->activeRequestId_ != requestId || self->exportState_ != state ||
                self->mode_ != PinWindowMode::Exporting)
                return;
            if (error.isEmpty())
                QApplication::clipboard()->setImage(image);
            else
                emit self->errorOccurred(self->id_, error);
            self->restoreOutputMode(requestId);
        }, Qt::QueuedConnection);
    }));
}

void PinWindow::beginSaveExport(QString path, annotation::AnnotationSnapshot snapshot,
                                const PinWindowMode priorMode, const std::uint64_t requestId) {
    if (!exportState_ || activeRequestId_ != requestId)
        return;
    outputPriorMode_ = priorMode;
    mode_ = PinWindowMode::Exporting;
    const auto state = exportState_;
    const QByteArray format = QFileInfo(path).suffix().compare(QStringLiteral("jpg"), Qt::CaseInsensitive) == 0 ||
                                      QFileInfo(path).suffix().compare(QStringLiteral("jpeg"), Qt::CaseInsensitive) == 0
                                  ? QByteArrayLiteral("jpeg")
                                  : QByteArrayLiteral("png");
    QPointer<PinWindow> self(this);
    exportPool_.start(QRunnable::create([self, path = std::move(path), snapshot = std::move(snapshot), state,
                                         requestId, format] {
        QString error;
        try {
            const QImage image = annotation::composeAnnotations(snapshot);
            error = lc::snip::saveImage(image, path, format, &state->cancelled,
                                        &state->finalizationMutex);
        } catch (const std::exception& exception) {
            error = QString::fromUtf8(exception.what());
        } catch (...) {
            error = QStringLiteral("保存贴图时发生未知错误。");
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, state, requestId, error] {
            if (!self || state->cancelled.load(std::memory_order_acquire) ||
                self->activeRequestId_ != requestId || self->exportState_ != state)
                return;
            if (!error.isEmpty())
                emit self->errorOccurred(self->id_, error);
            self->restoreOutputMode(requestId);
        }, Qt::QueuedConnection);
    }));
}

void PinWindow::restoreOutputMode(const std::uint64_t requestId) {
    if (activeRequestId_ != requestId || !exportState_ ||
        exportState_->cancelled.load(std::memory_order_acquire))
        return;
    exportState_.reset();
    mode_ = outputPriorMode_;
    toolbar_->setBusy(false);
    toolbar_->setVisible(mode_ == PinWindowMode::Editing);
    refresh();
}

void PinWindow::cancelExportAndWait() {
    const auto state = exportState_;
    if (state)
        state->cancelled.store(true, std::memory_order_release);
    ++nextRequestId_;
    activeRequestId_ = nextRequestId_;
    exportPool_.waitForDone();
    if (state) {
        std::lock_guard<std::mutex> lock(state->finalizationMutex);
    }
    exportState_.reset();
}

QTransform PinWindow::documentToWindowTransform() const {
    if (!document_ || document_->snapshot().base.isNull())
        return {};
    const auto size = document_->snapshot().base.size();
    return QTransform::fromScale(static_cast<qreal>(width()) / size.width(),
                                 static_cast<qreal>(height()) / size.height());
}

QPointF PinWindow::documentPoint(const QMouseEvent* event) const {
    bool invertible{};
    const auto inverse = documentToWindowTransform().inverted(&invertible);
    return invertible ? inverse.map(event->position()) : QPointF{};
}

void PinWindow::applyGeometry() {
    if (!geometry_)
        return;
    QWidget::setGeometry(geometry_->windowRect());
    setWindowOpacity(geometry_->opacity());
    positionToolbar();
    update();
}

void PinWindow::positionToolbar() {
    if (!toolbar_->isVisible())
        return;
    toolbar_->adjustSize();
    toolbar_->move(std::max(0, (width() - toolbar_->width()) / 2),
                   std::max(0, height() - toolbar_->height() - 8));
    toolbar_->raise();
}

void PinWindow::refresh() {
    if (document_ && interaction_) {
        toolbar_->setContext(document_.get(), interaction_.get());
        toolbar_->setContentAvailable(true);
    }
    positionToolbar();
    update();
}
} // namespace lc::pin
