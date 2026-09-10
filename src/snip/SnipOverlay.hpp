#pragma once

#include "snip/SelectionModel.hpp"
#include "snip/SnapshotImage.hpp"
#include "annotation/AnnotationTypes.hpp"

#include <QPointer>
#include <QRect>

#include <optional>

#include <QByteArray>
#include <QWidget>

class QKeyEvent;
class QCloseEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QToolButton;
class QDoubleSpinBox;
class QPlainTextEdit;

namespace lc::annotation {
class AnnotationDocument;
class AnnotationInteraction;
enum class AnnotationTool;
}

namespace lc::snip {
class SnipOverlay final : public QWidget {
    Q_OBJECT

  public:
    explicit SnipOverlay(FrozenMonitor monitor, SelectionModel& selection,
                         QWidget* parent = nullptr);

    void refresh();
    void setBusy(bool busy);
    void setToolbarHost(bool toolbarHost);
    void setAnnotationContext(annotation::AnnotationDocument* document,
                              annotation::AnnotationInteraction* interaction,
                              QRect lockedSelection);
    void clearAnnotationContext();

  signals:
    void selectionChanged();
    void copyRequested();
    void saveRequested();
    void cancelRequested();
    void displayInvalidated();
    void annotationChanged();
    void annotationToolRequested(annotation::AnnotationTool tool);
    void annotationUndoRequested();
    void annotationRedoRequested();
    void annotationDeleteRequested();

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
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    [[nodiscard]] QPoint physicalCursor(const QMouseEvent* event) const;
    [[nodiscard]] QRectF selectionInLocalCoordinates() const;
    [[nodiscard]] QPointF annotationPoint(const QMouseEvent* event) const;
    [[nodiscard]] QTransform documentToLocalTransform() const;
    [[nodiscard]] bool hasSelection() const noexcept;
    [[nodiscard]] bool annotating() const noexcept;
    [[nodiscard]] bool ownsToolbar() const;
    void beginTextEditor(QPointF anchor, std::optional<annotation::AnnotationObject> original = std::nullopt);
    void commitTextEditor();
    void cancelTextEditor();
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
    QToolButton* selectToolButton_{};
    QToolButton* rectangleToolButton_{};
    QToolButton* ellipseToolButton_{};
    QToolButton* arrowToolButton_{};
    QToolButton* brushToolButton_{};
    QToolButton* textToolButton_{};
    QToolButton* colorButton_{};
    QToolButton* undoButton_{};
    QToolButton* redoButton_{};
    QToolButton* deleteButton_{};
    QDoubleSpinBox* lineWidth_{};
    annotation::AnnotationDocument* document_{};
    annotation::AnnotationInteraction* interaction_{};
    QPointer<QPlainTextEdit> textEditor_;
    std::optional<annotation::AnnotationObject> textEditBefore_;
    annotation::AnnotationStyle textEditStyle_{Qt::red, 24.0};
    QPointF textEditAnchor_;
    QRect lockedSelection_;
    bool busy_{};
    bool dragging_{};
    bool placementComplete_{};
    bool toolbarHostAssigned_{};
    bool toolbarHost_{};
};
} // namespace lc::snip
