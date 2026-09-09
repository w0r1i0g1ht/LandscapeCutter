#include "snip/SnipOverlay.hpp"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
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

    toolbar_ = new QWidget(this);
    toolbar_->setObjectName(QStringLiteral("snipToolbar"));
    auto* layout = new QHBoxLayout(toolbar_);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    copyButton_ = new QToolButton(toolbar_);
    copyButton_->setObjectName(QStringLiteral("copyButton"));
    copyButton_->setFocusPolicy(Qt::NoFocus);
    copyButton_->setText(tr("复制"));
    layout->addWidget(copyButton_);

    saveButton_ = new QToolButton(toolbar_);
    saveButton_->setObjectName(QStringLiteral("saveButton"));
    saveButton_->setFocusPolicy(Qt::NoFocus);
    saveButton_->setText(tr("保存"));
    layout->addWidget(saveButton_);

    cancelButton_ = new QToolButton(toolbar_);
    cancelButton_->setObjectName(QStringLiteral("cancelButton"));
    cancelButton_->setFocusPolicy(Qt::NoFocus);
    cancelButton_->setText(tr("取消"));
    layout->addWidget(cancelButton_);

    connect(copyButton_, &QToolButton::clicked, this, &SnipOverlay::requestCopyIfSelected);
    connect(saveButton_, &QToolButton::clicked, this, &SnipOverlay::requestSaveIfSelected);
    connect(cancelButton_, &QToolButton::clicked, this, &SnipOverlay::cancelRequested);
    refresh();
}

void SnipOverlay::refresh() {
    const bool selected = hasSelection();
    const bool selectedOnThisMonitor =
        selected && selectionInLocalCoordinates().intersects(localMonitorRect(size()));
    copyButton_->setEnabled(selected && !busy_);
    saveButton_->setEnabled(selected && !busy_);
    cancelButton_->setEnabled(true);
    toolbar_->setVisible(selected && (toolbarHostAssigned_ ? toolbarHost_ : selectedOnThisMonitor));
    positionToolbar();
    update();
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
    refresh();
}

void SnipOverlay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawImage(rect(), monitor_.image);

    const auto selected = selectionInLocalCoordinates();
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
    const QString dimensions =
        QStringLiteral("%1 × %2").arg(selection_.rect().width()).arg(selection_.rect().height());
    painter.drawText(selected.topLeft() + QPointF{2.0, -4.0}, dimensions);

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
        emit cancelRequested();
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

    selection_.move(physicalCursor(event));
    selection_.release();
    dragging_ = false;
    releaseMouse();
    emit selectionChanged();
    refresh();
    event->accept();
}

void SnipOverlay::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!busy_ && event->button() == Qt::LeftButton && hasSelection()) {
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
    POINT cursor{};
    if (GetCursorPos(&cursor) != FALSE) {
        return {cursor.x, cursor.y};
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

bool SnipOverlay::hasSelection() const noexcept {
    return selection_.rect().isValid();
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
    if (hasSelection() && !busy_) {
        emit copyRequested();
    }
}

void SnipOverlay::requestSaveIfSelected() {
    if (hasSelection() && !busy_) {
        emit saveRequested();
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
