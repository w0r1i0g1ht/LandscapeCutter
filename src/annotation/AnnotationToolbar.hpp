#pragma once

#include "annotation/AnnotationTypes.hpp"

#include <QWidget>

class QDoubleSpinBox;
class QSpinBox;
class QToolButton;

namespace lc::annotation {
class AnnotationDocument;
class AnnotationInteraction;

enum class AnnotationToolbarMode { Snip, Pin };

class AnnotationToolbar final : public QWidget {
    Q_OBJECT

  public:
    explicit AnnotationToolbar(QWidget* parent = nullptr);

    void setMode(AnnotationToolbarMode mode);
    void setContentAvailable(bool available);
    void setContext(AnnotationDocument* document, AnnotationInteraction* interaction);
    void clearContext();
    void setBusy(bool busy);
    void refresh();
    [[nodiscard]] int textPixelSize() const noexcept;

  signals:
    void toolRequested(AnnotationTool tool);
    void annotationChanged();
    void undoRequested();
    void redoRequested();
    void deleteRequested();
    void copyRequested();
    void saveRequested();
    void pinRequested();
    void cancelRequested();
    void doneRequested();
    void textSizeChanged(int pixelSize);

  private:
    AnnotationToolbarMode mode_{AnnotationToolbarMode::Snip};
    AnnotationDocument* document_{};
    AnnotationInteraction* interaction_{};
    QToolButton* selectToolButton_{};
    QToolButton* rectangleToolButton_{};
    QToolButton* ellipseToolButton_{};
    QToolButton* arrowToolButton_{};
    QToolButton* brushToolButton_{};
    QToolButton* textToolButton_{};
    QToolButton* mosaicToolButton_{};
    QToolButton* colorButton_{};
    QToolButton* undoButton_{};
    QToolButton* redoButton_{};
    QToolButton* deleteButton_{};
    QToolButton* copyButton_{};
    QToolButton* saveButton_{};
    QToolButton* pinButton_{};
    QToolButton* cancelButton_{};
    QToolButton* doneButton_{};
    QDoubleSpinBox* lineWidth_{};
    QSpinBox* fontSize_{};
    QSpinBox* mosaicBlockSize_{};
    bool contentAvailable_{};
    bool busy_{};
};
} // namespace lc::annotation
