#pragma once

#include "snip/SelectionModel.hpp"
#include "snip/SnapshotImage.hpp"

#include <QByteArray>
#include <QWidget>

class QKeyEvent;
class QCloseEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QToolButton;

namespace lc::snip {
class SnipOverlay final : public QWidget {
    Q_OBJECT

  public:
    explicit SnipOverlay(FrozenMonitor monitor, SelectionModel& selection,
                         QWidget* parent = nullptr);

    void refresh();
    void setBusy(bool busy);
    void setToolbarHost(bool toolbarHost);

  signals:
    void selectionChanged();
    void copyRequested();
    void saveRequested();
    void cancelRequested();
    void displayInvalidated();

  protected:
    void closeEvent(QCloseEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

  private:
    [[nodiscard]] QPoint physicalCursor(const QMouseEvent* event) const;
    [[nodiscard]] QRectF selectionInLocalCoordinates() const;
    [[nodiscard]] bool hasSelection() const noexcept;
    void positionToolbar();
    void requestCopyIfSelected();
    void requestSaveIfSelected();
    [[nodiscard]] bool placeOnPhysicalMonitor();

    FrozenMonitor monitor_;
    SelectionModel& selection_;
    QWidget* toolbar_{};
    QToolButton* copyButton_{};
    QToolButton* saveButton_{};
    QToolButton* cancelButton_{};
    bool busy_{};
    bool dragging_{};
    bool placementComplete_{};
    bool toolbarHostAssigned_{};
    bool toolbarHost_{};
};
} // namespace lc::snip
