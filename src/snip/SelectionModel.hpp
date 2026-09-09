#pragma once

#include <QPoint>
#include <QRect>

namespace lc::snip {
enum class SelectionHit {
    None,
    Create,
    Move,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

class SelectionModel final {
  public:
    [[nodiscard]] QRect rect() const noexcept;

    void setBounds(QRect bounds);
    void press(QPoint point, int handleRadius);
    void move(QPoint point);
    void release() noexcept;
    void clear() noexcept;

    [[nodiscard]] SelectionHit hitTest(QPoint point, int handleRadius) const noexcept;

  private:
    [[nodiscard]] QPoint clampToBounds(QPoint point) const noexcept;
    void moveSelection(QPoint point) noexcept;
    void resizeSelection(QPoint point) noexcept;

    QRect bounds_;
    QRect rect_;
    QRect pressedRect_;
    QPoint pressedPoint_;
    SelectionHit pressedHit_{SelectionHit::None};
};
} // namespace lc::snip
