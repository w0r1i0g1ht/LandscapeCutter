#pragma once

#include "pin/PinGeometryModel.hpp"
#include "pin/PinTypes.hpp"
#include "annotation/AnnotationTypes.hpp"

#include <QWidget>
#include <QPointer>
#include <QTransform>

#include <memory>
#include <optional>

class QCloseEvent;
class QContextMenuEvent;
class QEvent;
class QAction;
class QKeyEvent;
class QMouseEvent;
class QMenu;
class QPaintEvent;
class QPlainTextEdit;
class QWheelEvent;

namespace lc::annotation {
class AnnotationDocument;
class AnnotationInteraction;
class AnnotationToolbar;
}

namespace lc::pin {
class PinWindow final : public QWidget {
    Q_OBJECT

  public:
    explicit PinWindow(PinId id, QWidget* parent = nullptr);
    ~PinWindow() override;

    QString attachDocument(std::unique_ptr<annotation::AnnotationDocument>& document,
                           QPoint preferredTopLeft);
    [[nodiscard]] annotation::AnnotationDocument* document() const noexcept;
    [[nodiscard]] PinWindowMode mode() const noexcept;
    [[nodiscard]] PinId id() const noexcept;
    void enterEditing();
    void finishEditing();

  signals:
    void copyRequested(PinId id);
    void saveRequested(PinId id);
    void closed(PinId id);

  protected:
    void closeEvent(QCloseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

  private:
    [[nodiscard]] QTransform documentToWindowTransform() const;
    [[nodiscard]] QPointF documentPoint(const QMouseEvent* event) const;
    void beginTextEditor(QPointF anchor,
                         std::optional<annotation::AnnotationObject> original = std::nullopt);
    void commitTextEditor();
    void cancelTextEditor();
    void editTextAt(QPointF point);
    void resetSize();
    void resetOpacity();
    void requestCopy();
    void requestSave();
    void applyGeometry();
    void positionToolbar();
    void refresh();

    PinId id_{};
    PinWindowMode mode_{PinWindowMode::Viewing};
    std::unique_ptr<annotation::AnnotationDocument> document_;
    std::unique_ptr<annotation::AnnotationInteraction> interaction_;
    annotation::AnnotationToolbar* toolbar_{};
    QAction* editAction_{};
    QAction* copyAction_{};
    QAction* saveAction_{};
    QAction* resetSizeAction_{};
    QAction* resetOpacityAction_{};
    QAction* closeAction_{};
    QMenu* contextMenu_{};
    std::optional<PinGeometryModel> geometry_;
    QPoint dragOffset_;
    bool dragging_{};
    bool closeEmitted_{};
    QPointer<QPlainTextEdit> textEditor_;
    std::optional<annotation::AnnotationObject> textEditBefore_;
    QPointF textEditAnchor_;
    annotation::AnnotationStyle textEditStyle_{};
};
} // namespace lc::pin
