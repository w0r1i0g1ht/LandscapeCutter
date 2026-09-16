#include "snip/SnipOverlay.hpp"

#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationInteraction.hpp"
#include "annotation/AnnotationRenderer.hpp"
#include "annotation/AnnotationToolbar.hpp"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <algorithm>
#include <array>

#ifdef Q_OS_WIN
#include <QtGui/qscreen_platform.h>
#endif

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace lc::snip {
namespace {
constexpr int kHandleRadius = 6;
constexpr int kToolbarMargin = 8;

[[nodiscard]] QRectF localMonitorRect(const QSize& localSize) {
    return {0.0, 0.0, static_cast<qreal>(localSize.width()),
            static_cast<qreal>(localSize.height())};
}
} // namespace

SnipOverlay::SnipOverlay(FrozenMonitor monitor, SelectionModel& selection, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      monitor_(std::move(monitor)), selection_(selection) {
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        for (QScreen* candidate : QGuiApplication::screens()) {
            const auto* native = candidate->nativeInterface<QNativeInterface::QWindowsScreen>();
            if (native != nullptr && native->handle() == monitor_.nativeHandle) {
                setScreen(candidate);
                break;
            }
        }
    }
#endif
    const qreal ratio = screen() != nullptr ? screen()->devicePixelRatio() : 1.0;
    resize(qMax(1, qRound(monitor_.geometry.width() / ratio)),
           qMax(1, qRound(monitor_.geometry.height() / ratio)));

    toolbar_ = new annotation::AnnotationToolbar(this);
    toolbar_->setObjectName(QStringLiteral("snipToolbar"));
    toolbar_->setMode(annotation::AnnotationToolbarMode::Snip);
    connect(toolbar_, &annotation::AnnotationToolbar::toolRequested, this,
            [this](annotation::AnnotationTool tool) {
                if (textEditor_ && interaction_ && tool != interaction_->tool())
                    commitTextEditor();
                emit annotationToolRequested(tool);
            });
    connect(toolbar_, &annotation::AnnotationToolbar::annotationChanged, this,
            &SnipOverlay::annotationChanged);
    connect(toolbar_, &annotation::AnnotationToolbar::textSizeChanged, this,
            [this](const int pixelSize) {
                if (!textEditor_)
                    return;
                textEditStyle_.physicalSize = pixelSize;
                if (textEditBefore_.has_value())
                    std::get<annotation::TextAnnotation>(textEditBefore_->payload)
                        .style.physicalSize = pixelSize;
                textEditor_->setFont(annotation::resolvedAnnotationFont(pixelSize));
                textEditor_->resize(
                    240, qMax(40, textEditor_->fontMetrics().lineSpacing() * 2));
            });
    connect(toolbar_, &annotation::AnnotationToolbar::undoRequested, this,
            &SnipOverlay::annotationUndoRequested);
    connect(toolbar_, &annotation::AnnotationToolbar::redoRequested, this,
            &SnipOverlay::annotationRedoRequested);
    connect(toolbar_, &annotation::AnnotationToolbar::deleteRequested, this,
            &SnipOverlay::annotationDeleteRequested);
    connect(toolbar_, &annotation::AnnotationToolbar::copyRequested, this,
            &SnipOverlay::requestCopyIfSelected);
    connect(toolbar_, &annotation::AnnotationToolbar::saveRequested, this,
            &SnipOverlay::requestSaveIfSelected);
    connect(toolbar_, &annotation::AnnotationToolbar::pinRequested, this,
            &SnipOverlay::requestPinIfSelected);
    connect(toolbar_, &annotation::AnnotationToolbar::cancelRequested, this,
            &SnipOverlay::cancelRequested);
    refresh();
}

void SnipOverlay::refresh() {
    const bool annotationActive = annotating();
    const bool selected = hasSelection();
    toolbar_->setContentAvailable(selected || annotationActive);
    toolbar_->setBusy(busy_);
    if (annotationActive)
        toolbar_->setContext(document_, interaction_);
    else
        toolbar_->clearContext();
    const bool ownToolbar = ownsToolbar();
    toolbar_->setVisible((selected || annotationActive) && ownToolbar);
    positionToolbar();
    update();
}

void SnipOverlay::setAnnotationContext(annotation::AnnotationDocument* document,
                                       annotation::AnnotationInteraction* interaction,
                                       QRect lockedSelection) {
    if (document_ != document || interaction_ != interaction)
        cancelTextEditor();
    document_ = document;
    interaction_ = interaction;
    lockedSelection_ = lockedSelection;
    refresh();
}

void SnipOverlay::clearAnnotationContext() {
    cancelTextEditor();
    document_ = nullptr;
    interaction_ = nullptr;
    lockedSelection_ = {};
    refresh();
}

bool SnipOverlay::isToolbarHost() const noexcept {
    return toolbarHostAssigned_ && toolbarHost_;
}

void SnipOverlay::createTextEditor(QPointF anchor) {
    beginTextEditor(anchor);
}

void SnipOverlay::editTextEditor(const annotation::AnnotationId id) {
    if (!annotating() || !ownsToolbar())
        return;
    const auto found = std::find_if(document_->objects().begin(), document_->objects().end(),
                                    [id](const auto& object) { return object.id == id; });
    if (found != document_->objects().end() &&
        std::holds_alternative<annotation::TextAnnotation>(found->payload)) {
        if (textEditor_) {
            if (textEditBefore_.has_value() || !textEditor_->toPlainText().trimmed().isEmpty())
                return;
            cancelTextEditor();
        }
        cancelAnnotationGesture();
        static_cast<void>(document_->select(id));
        beginTextEditor(std::get<annotation::TextAnnotation>(found->payload).anchor, *found);
        refresh();
    }
}

void SnipOverlay::setBusy(bool busy) {
    busy_ = busy;
    if (busy_ && dragging_) {
        selection_.release();
        dragging_ = false;
        releaseMouse();
    }
    refresh();
}

void SnipOverlay::setToolbarHost(const bool toolbarHost) {
    toolbarHostAssigned_ = true;
    toolbarHost_ = toolbarHost;
    if (!ownsToolbar())
        cancelTextEditor();
    refresh();
}

void SnipOverlay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawImage(rect(), monitor_.image);

    const bool annotationActive = annotating();
    if (annotationActive) {
        auto snapshot = document_->snapshot();
        if (const auto currentDraft = interaction_->draft(); currentDraft.has_value()) {
            const auto committed = std::find_if(
                snapshot.objects.begin(), snapshot.objects.end(), [currentDraft](const auto& object) {
                    return currentDraft->id != 0 && object.id == currentDraft->id;
                });
            if (committed == snapshot.objects.end())
                snapshot.objects.push_back(*currentDraft);
            else
                *committed = *currentDraft;
        }
        annotation::drawAnnotations(painter, snapshot, documentToLocalTransform(),
                                    localMonitorRect(size()));
    }

    const auto selected = annotationActive ? lockedSelectionInLocalCoordinates()
                                           : selectionInLocalCoordinates();
    if (selected.isEmpty() || !selected.intersects(localMonitorRect(size()))) {
        painter.fillRect(rect(), QColor(0, 0, 0, 120));
        return;
    }

    QPainterPath mask;
    mask.setFillRule(Qt::OddEvenFill);
    mask.addRect(localMonitorRect(size()));
    mask.addRect(selected);
    painter.fillPath(mask, QColor(0, 0, 0, 120));

    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(selected);
    const auto physicalSelection = annotationActive ? lockedSelection_ : selection_.rect();
    const QString dimensions = QStringLiteral("%1 × %2")
                                   .arg(physicalSelection.width())
                                   .arg(physicalSelection.height());
    painter.drawText(selected.topLeft() + QPointF{2.0, -4.0}, dimensions);

    if (annotationActive)
        return;

    painter.setBrush(Qt::white);
    const std::array<QPointF, 8> handles{
        selected.topLeft(),
        selected.topRight(),
        selected.bottomLeft(),
        selected.bottomRight(),
        QPointF{selected.left(), selected.center().y()},
        QPointF{selected.right(), selected.center().y()},
        QPointF{selected.center().x(), selected.top()},
        QPointF{selected.center().x(), selected.bottom()},
    };
    for (const auto& handle : handles) {
        painter.drawRect(QRectF{handle.x() - kHandleRadius / 2.0, handle.y() - kHandleRadius / 2.0,
                                static_cast<qreal>(kHandleRadius),
                                static_cast<qreal>(kHandleRadius)});
    }
}

void SnipOverlay::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    placementComplete_ = false;
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != QStringLiteral("windows")) {
        placementComplete_ = true;
        refresh();
        return;
    }
#endif
    static_cast<void>(placeOnPhysicalMonitor());
    QTimer::singleShot(0, this, [this] {
        if (!isVisible())
            return;
        const bool placed = placeOnPhysicalMonitor();
        placementComplete_ = true;
        if (!placed)
            emit displayInvalidated();
    });
    refresh();
}

void SnipOverlay::closeEvent(QCloseEvent* event) {
    emit cancelRequested();
    event->accept();
}

bool SnipOverlay::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (placementComplete_ && isVisible() &&
        (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG")) {
        const auto* nativeMessage = static_cast<const MSG*>(message);
        if (nativeMessage != nullptr && nativeMessage->message == WM_DPICHANGED) {
            emit displayInvalidated();
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void SnipOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    refresh();
}

void SnipOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (annotating() && interaction_->hasDraft()) {
            interaction_->cancelDraft();
            emit annotationChanged();
            refresh();
            event->accept();
            return;
        }
        emit cancelRequested();
        event->accept();
        return;
    }
    if (annotating() && event->key() == Qt::Key_Z && event->modifiers().testFlag(Qt::ControlModifier)) {
        if (event->modifiers().testFlag(Qt::ShiftModifier))
            emit annotationRedoRequested();
        else
            emit annotationUndoRequested();
        event->accept();
        return;
    }
    if (annotating() && event->key() == Qt::Key_Y && event->modifiers().testFlag(Qt::ControlModifier)) {
        emit annotationRedoRequested();
        event->accept();
        return;
    }
    if (annotating() && event->key() == Qt::Key_Delete) {
        emit annotationDeleteRequested();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
        (event->key() == Qt::Key_C && event->modifiers().testFlag(Qt::ControlModifier))) {
        requestCopyIfSelected();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_S && event->modifiers().testFlag(Qt::ControlModifier)) {
        requestSaveIfSelected();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SnipOverlay::mousePressEvent(QMouseEvent* event) {
    if (busy_) {
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (annotating() && interaction_->tool() == annotation::AnnotationTool::Text) {
        const auto hit = interaction_->hitTest(annotationPoint(event));
        const auto found = hit.has_value()
                               ? std::find_if(document_->objects().begin(), document_->objects().end(),
                                              [hit](const auto& object) { return object.id == *hit; })
                               : document_->objects().end();
        if (found != document_->objects().end() &&
            std::holds_alternative<annotation::TextAnnotation>(found->payload)) {
            cancelAnnotationGesture();
            if (ownsToolbar())
                editTextEditor(*hit);
            else
                emit annotationTextEditRequested(*hit);
        } else if (!textEditor_) {
            if (ownsToolbar())
                createTextEditor(annotationPoint(event));
            else
                emit annotationTextCreateRequested(annotationPoint(event));
        }
        event->accept();
        return;
    }

    if (annotating() && textEditor_) {
        event->accept();
        return;
    }

    if (annotating()) {
        interaction_->press(annotationPoint(event));
        dragging_ = true;
        grabMouse();
        emit annotationChanged();
        refresh();
        event->accept();
        return;
    }

    selection_.press(physicalCursor(event), kHandleRadius);
    dragging_ = true;
    grabMouse();
    emit selectionChanged();
    refresh();
    event->accept();
}

void SnipOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (busy_) {
        event->accept();
        return;
    }
    if (!dragging_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (annotating()) {
        interaction_->move(annotationPoint(event));
        emit annotationChanged();
        refresh();
        event->accept();
        return;
    }

    selection_.move(physicalCursor(event));
    emit selectionChanged();
    refresh();
    event->accept();
}

void SnipOverlay::mouseReleaseEvent(QMouseEvent* event) {
    if (busy_) {
        event->accept();
        return;
    }
    if (!dragging_ || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (annotating()) {
        interaction_->release(annotationPoint(event));
        dragging_ = false;
        releaseMouse();
        emit annotationChanged();
        refresh();
        event->accept();
        return;
    }

    selection_.move(physicalCursor(event));
    selection_.release();
    dragging_ = false;
    releaseMouse();
    emit selectionChanged();
    refresh();
    event->accept();
}

void SnipOverlay::mouseDoubleClickEvent(QMouseEvent* event) {
    if (annotating() && !busy_ && event->button() == Qt::LeftButton && ownsToolbar()) {
        const auto hit = interaction_->hitTest(annotationPoint(event));
        if (hit.has_value()) {
            const auto found = std::find_if(document_->objects().begin(), document_->objects().end(),
                                            [hit](const auto& object) { return object.id == *hit; });
            if (found != document_->objects().end() &&
                std::holds_alternative<annotation::TextAnnotation>(found->payload)) {
                cancelAnnotationGesture();
                editTextEditor(*hit);
                event->accept();
                return;
            }
        }
    } else if (annotating() && !busy_ && event->button() == Qt::LeftButton) {
        const auto hit = interaction_->hitTest(annotationPoint(event));
        if (hit.has_value()) {
            const auto found = std::find_if(document_->objects().begin(), document_->objects().end(),
                                            [hit](const auto& object) { return object.id == *hit; });
            if (found != document_->objects().end() &&
                std::holds_alternative<annotation::TextAnnotation>(found->payload)) {
                cancelAnnotationGesture();
                emit annotationTextEditRequested(*hit);
                event->accept();
                return;
            }
        }
    }
    if (!annotating() && !busy_ && event->button() == Qt::LeftButton && hasSelection()) {
        const auto point = physicalCursor(event);
        const auto rectangle = selection_.rect();
        if (point.x() >= rectangle.x() && point.x() < rectangle.x() + rectangle.width() &&
            point.y() >= rectangle.y() && point.y() < rectangle.y() + rectangle.height()) {
            requestCopyIfSelected();
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

QPoint SnipOverlay::physicalCursor(const QMouseEvent* event) const {
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        POINT cursor{};
        if (GetCursorPos(&cursor) != FALSE) {
            return {cursor.x, cursor.y};
        }
    }
#endif
    const qreal horizontalScale = monitor_.geometry.width() > 0
                                      ? static_cast<qreal>(monitor_.geometry.width()) / width()
                                      : 1.0;
    const qreal verticalScale = monitor_.geometry.height() > 0
                                    ? static_cast<qreal>(monitor_.geometry.height()) / height()
                                    : 1.0;
    return {monitor_.geometry.x() + static_cast<int>(event->position().x() * horizontalScale),
            monitor_.geometry.y() + static_cast<int>(event->position().y() * verticalScale)};
}

QRectF SnipOverlay::selectionInLocalCoordinates() const {
    const auto selection = selection_.rect();
    if (!selection.isValid() || monitor_.geometry.width() <= 0 || monitor_.geometry.height() <= 0) {
        return {};
    }

    const qreal horizontalScale = static_cast<qreal>(width()) / monitor_.geometry.width();
    const qreal verticalScale = static_cast<qreal>(height()) / monitor_.geometry.height();
    return {(selection.x() - monitor_.geometry.x()) * horizontalScale,
            (selection.y() - monitor_.geometry.y()) * verticalScale,
            selection.width() * horizontalScale, selection.height() * verticalScale};
}

QRectF SnipOverlay::lockedSelectionInLocalCoordinates() const {
    if (!lockedSelection_.isValid() || monitor_.geometry.width() <= 0 ||
        monitor_.geometry.height() <= 0) {
        return {};
    }

    const qreal horizontalScale = static_cast<qreal>(width()) / monitor_.geometry.width();
    const qreal verticalScale = static_cast<qreal>(height()) / monitor_.geometry.height();
    return {(lockedSelection_.x() - monitor_.geometry.x()) * horizontalScale,
            (lockedSelection_.y() - monitor_.geometry.y()) * verticalScale,
            lockedSelection_.width() * horizontalScale,
            lockedSelection_.height() * verticalScale};
}

QPointF SnipOverlay::annotationPoint(const QMouseEvent* event) const {
    return QPointF(physicalCursor(event) - lockedSelection_.topLeft());
}

QTransform SnipOverlay::documentToLocalTransform() const {
    const qreal horizontalScale = monitor_.geometry.width() > 0
                                      ? static_cast<qreal>(width()) / monitor_.geometry.width()
                                      : 1.0;
    const qreal verticalScale = monitor_.geometry.height() > 0
                                    ? static_cast<qreal>(height()) / monitor_.geometry.height()
                                    : 1.0;
    return {horizontalScale, 0.0, 0.0, verticalScale,
            (lockedSelection_.x() - monitor_.geometry.x()) * horizontalScale,
            (lockedSelection_.y() - monitor_.geometry.y()) * verticalScale};
}

bool SnipOverlay::hasSelection() const noexcept {
    return selection_.rect().isValid();
}

bool SnipOverlay::annotating() const noexcept {
    return document_ != nullptr && interaction_ != nullptr && !lockedSelection_.isEmpty();
}

bool SnipOverlay::ownsToolbar() const {
    return toolbarHostAssigned_
               ? toolbarHost_
               : (hasSelection() && selectionInLocalCoordinates().intersects(localMonitorRect(size())));
}

void SnipOverlay::cancelAnnotationGesture() {
    if (interaction_)
        interaction_->cancelDraft();
    if (dragging_) {
        dragging_ = false;
        releaseMouse();
    }
}

void SnipOverlay::beginTextEditor(QPointF anchor,
                                  std::optional<annotation::AnnotationObject> original) {
    if (!annotating() || !ownsToolbar() || textEditor_)
        return;
    textEditBefore_ = std::move(original);
    textEditAnchor_ = anchor;
    textEditStyle_ = textEditBefore_.has_value()
                         ? std::get<annotation::TextAnnotation>(textEditBefore_->payload).style
                         : annotation::AnnotationStyle{interaction_->style().color,
                                                       static_cast<qreal>(toolbar_->textPixelSize())};
    auto* editor = new QPlainTextEdit(this);
    textEditor_ = editor;
    editor->setObjectName(QStringLiteral("annotationTextEditor"));
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFont(annotation::resolvedAnnotationFont(qMax(1, qRound(textEditStyle_.physicalSize))));
    editor->setPlainText(textEditBefore_.has_value()
                             ? std::get<annotation::TextAnnotation>(textEditBefore_->payload).text
                             : QString{});
    editor->installEventFilter(this);
    const QPointF localAnchor = documentToLocalTransform().map(anchor);
    editor->move(qRound(localAnchor.x()), qRound(localAnchor.y()));
    editor->resize(240, qMax(40, editor->fontMetrics().lineSpacing() * 2));
    editor->show();
    editor->setFocus(Qt::OtherFocusReason);
    editor->raise();
}

void SnipOverlay::commitTextEditor() {
    if (!textEditor_ || !document_)
        return;
    const QString text = textEditor_->toPlainText();
    if (textEditBefore_.has_value()) {
        auto replacement = *textEditBefore_;
        auto& annotation = std::get<annotation::TextAnnotation>(replacement.payload);
        annotation.text = text;
        static_cast<void>(document_->replaceObject(std::move(replacement)));
    } else {
        static_cast<void>(document_->addObject(annotation::TextAnnotation{textEditAnchor_, text, textEditStyle_}));
    }
    cancelTextEditor();
    emit annotationChanged();
    refresh();
}

void SnipOverlay::cancelTextEditor() {
    if (textEditor_) {
        auto* editor = textEditor_.data();
        textEditor_.clear();
        editor->removeEventFilter(this);
        delete editor;
    }
    textEditBefore_.reset();
}

bool SnipOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (watched == textEditor_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            key->modifiers().testFlag(Qt::ControlModifier)) {
            commitTextEditor();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            cancelTextEditor();
            emit annotationChanged();
            refresh();
            return true;
        }
        if (key->key() == Qt::Key_S && key->modifiers().testFlag(Qt::ControlModifier))
            return true;
    }
    return QWidget::eventFilter(watched, event);
}

void SnipOverlay::positionToolbar() {
    if (!toolbar_->isVisible()) {
        return;
    }

    toolbar_->adjustSize();
    const auto selected = selectionInLocalCoordinates().intersected(localMonitorRect(size()));
    const auto preferredX = selected.isEmpty() ? kToolbarMargin : static_cast<int>(selected.left());
    const auto preferredY =
        selected.isEmpty() ? kToolbarMargin : static_cast<int>(selected.bottom()) + kToolbarMargin;
    const auto maximumX = std::max(0, width() - toolbar_->width());
    const auto maximumY = std::max(0, height() - toolbar_->height());
    toolbar_->move(std::clamp(preferredX, 0, maximumX), std::clamp(preferredY, 0, maximumY));
    toolbar_->raise();
}

void SnipOverlay::requestCopyIfSelected() {
    if ((hasSelection() || annotating()) && !busy_) {
        if (textEditor_)
            commitTextEditor();
        emit copyRequested();
    }
}

void SnipOverlay::requestSaveIfSelected() {
    if ((hasSelection() || annotating()) && !busy_) {
        if (textEditor_)
            commitTextEditor();
        emit saveRequested();
    }
}

void SnipOverlay::requestPinIfSelected() {
    if ((hasSelection() || annotating()) && !busy_) {
        if (textEditor_)
            commitTextEditor();
        emit pinRequested();
    }
}

bool SnipOverlay::placeOnPhysicalMonitor() {
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        const auto handle = reinterpret_cast<HWND>(winId());
        if (SetWindowPos(handle, HWND_TOPMOST, monitor_.geometry.x(), monitor_.geometry.y(),
                         monitor_.geometry.width(), monitor_.geometry.height(),
                         SWP_NOACTIVATE | SWP_SHOWWINDOW) == FALSE) {
            return false;
        }
        RECT actual{};
        return GetWindowRect(handle, &actual) != FALSE && actual.left == monitor_.geometry.left() &&
               actual.top == monitor_.geometry.top() &&
               actual.right == monitor_.geometry.left() + monitor_.geometry.width() &&
               actual.bottom == monitor_.geometry.top() + monitor_.geometry.height();
    }
#endif
    return true;
}
} // namespace lc::snip
