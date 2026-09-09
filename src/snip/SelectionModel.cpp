#include "snip/SelectionModel.hpp"

#include <algorithm>
#include <cstdint>

namespace lc::snip {
namespace {
using Coordinate = std::int64_t;

[[nodiscard]] Coordinate left(const QRect& rect) noexcept {
    return rect.x();
}
[[nodiscard]] Coordinate top(const QRect& rect) noexcept {
    return rect.y();
}
[[nodiscard]] Coordinate right(const QRect& rect) noexcept {
    return left(rect) + static_cast<Coordinate>(rect.width());
}
[[nodiscard]] Coordinate bottom(const QRect& rect) noexcept {
    return top(rect) + static_cast<Coordinate>(rect.height());
}

[[nodiscard]] bool isValid(const QRect& rect) noexcept {
    return rect.width() > 0 && rect.height() > 0;
}

[[nodiscard]] int asInt(Coordinate coordinate) noexcept {
    return static_cast<int>(coordinate);
}

[[nodiscard]] QRect fromEdges(Coordinate rectangleLeft, Coordinate rectangleTop,
                              Coordinate rectangleRight, Coordinate rectangleBottom) noexcept {
    return {asInt(rectangleLeft), asInt(rectangleTop), asInt(rectangleRight - rectangleLeft),
            asInt(rectangleBottom - rectangleTop)};
}

[[nodiscard]] bool isNear(Coordinate value, Coordinate target, Coordinate radius) noexcept {
    return value >= target - radius && value <= target + radius;
}
} // namespace

QRect SelectionModel::rect() const noexcept {
    return rect_;
}

void SelectionModel::setBounds(QRect bounds) {
    bounds_ = isValid(bounds) ? bounds : QRect{};
    if (!isValid(rect_) || !isValid(bounds_)) {
        clear();
        return;
    }

    const auto clippedLeft = std::max(left(rect_), left(bounds_));
    const auto clippedTop = std::max(top(rect_), top(bounds_));
    const auto clippedRight = std::min(right(rect_), right(bounds_));
    const auto clippedBottom = std::min(bottom(rect_), bottom(bounds_));
    rect_ = clippedLeft < clippedRight && clippedTop < clippedBottom
                ? fromEdges(clippedLeft, clippedTop, clippedRight, clippedBottom)
                : QRect{};
}

void SelectionModel::press(QPoint point, int handleRadius) {
    pressedHit_ = hitTest(point, handleRadius);
    pressedRect_ = rect_;
    pressedPoint_ = clampToBounds(point);

    if (pressedHit_ == SelectionHit::Create) {
        rect_ = {};
    }
}

void SelectionModel::move(QPoint point) {
    if (pressedHit_ == SelectionHit::None || !isValid(bounds_)) {
        return;
    }

    const auto current = clampToBounds(point);
    if (pressedHit_ == SelectionHit::Create) {
        const auto rectangleLeft = std::min<Coordinate>(pressedPoint_.x(), current.x());
        const auto rectangleTop = std::min<Coordinate>(pressedPoint_.y(), current.y());
        const auto rectangleRight = std::max<Coordinate>(pressedPoint_.x(), current.x());
        const auto rectangleBottom = std::max<Coordinate>(pressedPoint_.y(), current.y());
        rect_ = rectangleLeft < rectangleRight && rectangleTop < rectangleBottom
                    ? fromEdges(rectangleLeft, rectangleTop, rectangleRight, rectangleBottom)
                    : QRect{};
        return;
    }

    if (!isValid(pressedRect_)) {
        return;
    }

    if (pressedHit_ == SelectionHit::Move) {
        moveSelection(current);
        return;
    }

    resizeSelection(current);
}

void SelectionModel::release() noexcept {
    pressedHit_ = SelectionHit::None;
}

void SelectionModel::clear() noexcept {
    rect_ = {};
    pressedRect_ = {};
    pressedHit_ = SelectionHit::None;
}

SelectionHit SelectionModel::hitTest(QPoint point, int handleRadius) const noexcept {
    if (!isValid(bounds_)) {
        return SelectionHit::None;
    }
    if (!isValid(rect_)) {
        return SelectionHit::Create;
    }

    const auto radius = std::max(0, handleRadius);
    const auto pointX = static_cast<Coordinate>(point.x());
    const auto pointY = static_cast<Coordinate>(point.y());
    const auto rectangleLeft = left(rect_);
    const auto rectangleTop = top(rect_);
    const auto rectangleRight = right(rect_);
    const auto rectangleBottom = bottom(rect_);
    const auto hitRadius = static_cast<Coordinate>(radius);

    const bool nearLeft = isNear(pointX, rectangleLeft, hitRadius);
    const bool nearRight = isNear(pointX, rectangleRight, hitRadius);
    const bool nearTop = isNear(pointY, rectangleTop, hitRadius);
    const bool nearBottom = isNear(pointY, rectangleBottom, hitRadius);
    if (nearLeft && nearTop) {
        return SelectionHit::TopLeft;
    }
    if (nearRight && nearTop) {
        return SelectionHit::TopRight;
    }
    if (nearLeft && nearBottom) {
        return SelectionHit::BottomLeft;
    }
    if (nearRight && nearBottom) {
        return SelectionHit::BottomRight;
    }

    if (nearLeft && pointY >= rectangleTop && pointY <= rectangleBottom) {
        return SelectionHit::Left;
    }
    if (nearRight && pointY >= rectangleTop && pointY <= rectangleBottom) {
        return SelectionHit::Right;
    }
    if (nearTop && pointX >= rectangleLeft && pointX <= rectangleRight) {
        return SelectionHit::Top;
    }
    if (nearBottom && pointX >= rectangleLeft && pointX <= rectangleRight) {
        return SelectionHit::Bottom;
    }

    if (pointX >= rectangleLeft && pointX < rectangleRight && pointY >= rectangleTop &&
        pointY < rectangleBottom) {
        return SelectionHit::Move;
    }
    return SelectionHit::Create;
}

QPoint SelectionModel::clampToBounds(QPoint point) const noexcept {
    const auto minimumX = left(bounds_);
    const auto minimumY = top(bounds_);
    const auto maximumX = right(bounds_);
    const auto maximumY = bottom(bounds_);
    return {asInt(std::clamp(static_cast<Coordinate>(point.x()), minimumX, maximumX)),
            asInt(std::clamp(static_cast<Coordinate>(point.y()), minimumY, maximumY))};
}

void SelectionModel::moveSelection(QPoint point) noexcept {
    const auto width = static_cast<Coordinate>(pressedRect_.width());
    const auto height = static_cast<Coordinate>(pressedRect_.height());
    const auto deltaX = static_cast<Coordinate>(point.x()) - pressedPoint_.x();
    const auto deltaY = static_cast<Coordinate>(point.y()) - pressedPoint_.y();
    const auto rectangleLeft =
        std::clamp(left(pressedRect_) + deltaX, left(bounds_), right(bounds_) - width);
    const auto rectangleTop =
        std::clamp(top(pressedRect_) + deltaY, top(bounds_), bottom(bounds_) - height);
    rect_ = fromEdges(rectangleLeft, rectangleTop, rectangleLeft + width, rectangleTop + height);
}

void SelectionModel::resizeSelection(QPoint point) noexcept {
    auto rectangleLeft = left(pressedRect_);
    auto rectangleTop = top(pressedRect_);
    auto rectangleRight = right(pressedRect_);
    auto rectangleBottom = bottom(pressedRect_);
    const auto pointX = static_cast<Coordinate>(point.x());
    const auto pointY = static_cast<Coordinate>(point.y());

    switch (pressedHit_) {
    case SelectionHit::Left:
    case SelectionHit::TopLeft:
    case SelectionHit::BottomLeft:
        rectangleLeft = std::clamp(pointX, left(bounds_), rectangleRight - 1);
        break;
    default:
        break;
    }
    switch (pressedHit_) {
    case SelectionHit::Right:
    case SelectionHit::TopRight:
    case SelectionHit::BottomRight:
        rectangleRight = std::clamp(pointX, rectangleLeft + 1, right(bounds_));
        break;
    default:
        break;
    }
    switch (pressedHit_) {
    case SelectionHit::Top:
    case SelectionHit::TopLeft:
    case SelectionHit::TopRight:
        rectangleTop = std::clamp(pointY, top(bounds_), rectangleBottom - 1);
        break;
    default:
        break;
    }
    switch (pressedHit_) {
    case SelectionHit::Bottom:
    case SelectionHit::BottomLeft:
    case SelectionHit::BottomRight:
        rectangleBottom = std::clamp(pointY, rectangleTop + 1, bottom(bounds_));
        break;
    default:
        break;
    }
    rect_ = fromEdges(rectangleLeft, rectangleTop, rectangleRight, rectangleBottom);
}
} // namespace lc::snip
