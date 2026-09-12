#pragma once

#include "annotation/AnnotationDocument.hpp"

#include <optional>

namespace lc::annotation {
class AnnotationInteraction final {
  public:
    explicit AnnotationInteraction(AnnotationDocument& document);

    void setTool(AnnotationTool tool) noexcept;
    [[nodiscard]] AnnotationTool tool() const noexcept;
    void setStyle(AnnotationStyle style);
    [[nodiscard]] AnnotationStyle style() const noexcept;
    void setMosaicBlockSize(int blockSize) noexcept;
    [[nodiscard]] int mosaicBlockSize() const noexcept;

    void press(QPointF point);
    void move(QPointF point);
    void release(QPointF point);
    void cancelDraft() noexcept;
    [[nodiscard]] bool deleteSelection();

    [[nodiscard]] std::optional<AnnotationId> hitTest(QPointF point) const;
    [[nodiscard]] std::optional<AnnotationObject> draft() const;
    [[nodiscard]] bool hasDraft() const noexcept;

  private:
    enum class EditMode { None, Move, ResizeTopLeft, ResizeTopRight, ResizeBottomLeft,
                          ResizeBottomRight, ArrowStart, ArrowEnd };

    [[nodiscard]] QRectF bounds() const;
    [[nodiscard]] QPointF clip(QPointF point) const;
    void createDraft(QPointF point);
    void updateDraft(QPointF point);
    void updateEdit(QPointF point);
    void appendFreehandPoint(QPointF point, bool force);
    [[nodiscard]] EditMode editModeFor(const AnnotationObject& object, QPointF point) const;

    AnnotationDocument& document_;
    AnnotationTool tool_{AnnotationTool::Select};
    AnnotationStyle style_{};
    int mosaicBlockSize_{12};
    std::optional<AnnotationObject> draft_;
    std::optional<AnnotationObject> editBefore_;
    QPointF pressedPoint_;
    EditMode editMode_{EditMode::None};
};
} // namespace lc::annotation
