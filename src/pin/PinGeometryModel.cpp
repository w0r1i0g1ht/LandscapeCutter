#include "pin/PinGeometryModel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lc::pin {
namespace {
constexpr int kMinimumEdge = 32;

qint64 squaredDistanceTo(const QPoint point, const QRect& rect) noexcept {
    const auto nearestX = std::clamp(point.x(), rect.left(), rect.right());
    const auto nearestY = std::clamp(point.y(), rect.top(), rect.bottom());
    const auto dx = static_cast<qint64>(point.x()) - nearestX;
    const auto dy = static_cast<qint64>(point.y()) - nearestY;
    return dx * dx + dy * dy;
}
} // namespace

PinGeometryModel::PinGeometryModel(QSize documentSize, QRect windowRect)
    : documentSize_(documentSize), windowRect_(windowRect),
      valid_(documentSize.width() > 0 && documentSize.height() > 0 && windowRect.width() > 0 &&
             windowRect.height() > 0) {
    if (!valid_) {
        documentSize_ = {};
        windowRect_ = {};
    }
}

void PinGeometryModel::moveTo(const QPoint topLeft) noexcept {
    if (valid_)
        windowRect_.moveTopLeft(topLeft);
}

void PinGeometryModel::zoomAt(const QPoint desktopAnchor, const int angleDeltaY) noexcept {
    if (!valid_ || angleDeltaY == 0)
        return;

    const auto anchorX = std::clamp(
        static_cast<qreal>(desktopAnchor.x() - windowRect_.left()) / windowRect_.width(), 0.0,
        1.0);
    const auto anchorY = std::clamp(
        static_cast<qreal>(desktopAnchor.y() - windowRect_.top()) / windowRect_.height(), 0.0,
        1.0);
    const auto currentScale =
        static_cast<qreal>(windowRect_.width()) / static_cast<qreal>(documentSize_.width());
    const auto requestedScale = currentScale * std::pow(1.1, angleDeltaY / 120.0);
    const auto maximumExtent = static_cast<qreal>(std::numeric_limits<int>::max() / 4);
    const auto maximumScale =
        std::min(maximumExtent / documentSize_.width(), maximumExtent / documentSize_.height());
    if (!std::isfinite(requestedScale) || requestedScale > maximumScale)
        return;
    const auto minimumScale =
        std::max(static_cast<qreal>(kMinimumEdge) / documentSize_.width(),
                 static_cast<qreal>(kMinimumEdge) / documentSize_.height());
    const auto scale = std::max(requestedScale, minimumScale);
    const auto width = std::max(1, qRound(documentSize_.width() * scale));
    const auto height = std::max(1, qRound(documentSize_.height() * scale));
    const auto left = qRound(desktopAnchor.x() - anchorX * width);
    const auto top = qRound(desktopAnchor.y() - anchorY * height);
    windowRect_ = {left, top, width, height};
}

void PinGeometryModel::adjustOpacity(const int angleDeltaY) noexcept {
    if (!valid_)
        return;
    const auto steps = angleDeltaY / 120;
    if (steps == 0)
        return;
    opacity_ = std::clamp(opacity_ + static_cast<qreal>(steps) * 0.05, 0.10, 1.00);
}

void PinGeometryModel::resetSize() noexcept {
    if (valid_)
        windowRect_.setSize(documentSize_);
}

bool PinGeometryModel::ensureOperable(const QList<QRect>& availableGeometries) noexcept {
    if (!valid_ || availableGeometries.isEmpty())
        return false;

    const auto requiredWidth = std::min(kMinimumEdge, windowRect_.width());
    const auto requiredHeight = std::min(kMinimumEdge, windowRect_.height());
    for (const auto& screen : availableGeometries) {
        const auto intersection = windowRect_.intersected(screen);
        if (intersection.width() >= requiredWidth && intersection.height() >= requiredHeight)
            return false;
    }

    const auto center = windowRect_.center();
    const QRect* nearest = nullptr;
    auto nearestDistance = std::numeric_limits<qint64>::max();
    for (const auto& screen : availableGeometries) {
        if (screen.isEmpty())
            continue;
        const auto distance = squaredDistanceTo(center, screen);
        if (distance < nearestDistance) {
            nearest = &screen;
            nearestDistance = distance;
        }
    }
    if (!nearest)
        return false;

    const auto maximumX = nearest->width() >= windowRect_.width()
                              ? nearest->right() - windowRect_.width() + 1
                              : nearest->left();
    const auto maximumY = nearest->height() >= windowRect_.height()
                              ? nearest->bottom() - windowRect_.height() + 1
                              : nearest->top();
    windowRect_.moveTopLeft(
        {std::clamp(windowRect_.left(), nearest->left(), maximumX),
         std::clamp(windowRect_.top(), nearest->top(), maximumY)});
    return true;
}

bool PinGeometryModel::valid() const noexcept {
    return valid_;
}

QRect PinGeometryModel::windowRect() const noexcept {
    return windowRect_;
}

qreal PinGeometryModel::opacity() const noexcept {
    return opacity_;
}
} // namespace lc::pin
