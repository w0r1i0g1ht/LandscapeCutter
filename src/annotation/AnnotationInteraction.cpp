#include "annotation/AnnotationInteraction.hpp"
#include "annotation/AnnotationRenderer.hpp"

#include <QLineF>

#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>

namespace lc::annotation {
namespace {
constexpr qreal kHandleTolerance = 6.0;
constexpr qreal kFreehandMergeDistance = 1.5;
constexpr qreal kTextHitTolerance = 3.0;

qreal lineTolerance(const AnnotationStyle& style) {
    return std::max<qreal>(6.0, style.physicalSize / 2.0 + 3.0);
}

qreal pointToSegmentDistance(QPointF point, QPointF start, QPointF end) {
    const auto dx = end.x() - start.x();
    const auto dy = end.y() - start.y();
    const auto lengthSquared = dx * dx + dy * dy;
    if (lengthSquared == 0.0) {
        return QLineF(point, start).length();
    }
    const auto fraction = std::clamp(((point.x() - start.x()) * dx +
                                      (point.y() - start.y()) * dy) /
                                         lengthSquared,
                                     0.0, 1.0);
    return QLineF(point, {start.x() + fraction * dx, start.y() + fraction * dy}).length();
}

QRectF payloadBounds(const AnnotationPayload& payload) {
    return std::visit(
        [](const auto& value) -> QRectF {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                          std::is_same_v<Value, EllipseAnnotation> ||
                          std::is_same_v<Value, MosaicAnnotation>) {
                return value.rect.normalized();
            } else if constexpr (std::is_same_v<Value, ArrowAnnotation>) {
                return QRectF(value.start, value.end).normalized();
            } else if constexpr (std::is_same_v<Value, FreehandAnnotation>) {
                if (value.points.empty())
                    return {};
                qreal minimumX = value.points.front().x();
                qreal maximumX = minimumX;
                qreal minimumY = value.points.front().y();
                qreal maximumY = minimumY;
                for (const auto& point : value.points) {
                    minimumX = std::min(minimumX, point.x());
                    maximumX = std::max(maximumX, point.x());
                    minimumY = std::min(minimumY, point.y());
                    maximumY = std::max(maximumY, point.y());
                }
                return {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
            } else if constexpr (std::is_same_v<Value, TextAnnotation>) {
                return textLogicalRect(value);
            } else {
                return QRectF(value.anchor, QSizeF{});
            }
        },
        payload);
}

void translatePayload(AnnotationPayload& payload, QPointF delta) {
    std::visit(
        [delta](auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                          std::is_same_v<Value, EllipseAnnotation> ||
                          std::is_same_v<Value, MosaicAnnotation>) {
                value.rect.translate(delta);
            } else if constexpr (std::is_same_v<Value, ArrowAnnotation>) {
                value.start += delta;
                value.end += delta;
            } else if constexpr (std::is_same_v<Value, FreehandAnnotation>) {
                for (auto& point : value.points)
                    point += delta;
            } else {
                value.anchor += delta;
            }
        },
        payload);
}
} // namespace

AnnotationInteraction::AnnotationInteraction(AnnotationDocument& document) : document_(document) {}

void AnnotationInteraction::setTool(AnnotationTool tool) noexcept {
    cancelDraft();
    tool_ = tool;
}

AnnotationTool AnnotationInteraction::tool() const noexcept {
    return tool_;
}

void AnnotationInteraction::setStyle(AnnotationStyle style) {
    style_ = style;
    const auto selected = document_.selectedId();
    if (!selected.has_value())
        return;
    const auto found = std::find_if(document_.objects().begin(), document_.objects().end(),
                                    [selected](const AnnotationObject& object) {
                                        return object.id == *selected;
                                    });
    if (found == document_.objects().end())
        return;
    auto replacement = *found;
    const bool supported = std::visit(
        [&style](auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                          std::is_same_v<Value, EllipseAnnotation> ||
                          std::is_same_v<Value, ArrowAnnotation> ||
                          std::is_same_v<Value, FreehandAnnotation> ||
                          std::is_same_v<Value, TextAnnotation>) {
                value.style = style;
                return true;
            }
            return false;
        },
        replacement.payload);
    if (supported)
        static_cast<void>(document_.replaceObject(std::move(replacement)));
}

AnnotationStyle AnnotationInteraction::style() const noexcept {
    return style_;
}

void AnnotationInteraction::setMosaicBlockSize(int blockSize) noexcept {
    mosaicBlockSize_ = std::clamp(blockSize, 1, 128);
    const auto selected = document_.selectedId();
    if (!selected.has_value())
        return;
    const auto found = std::find_if(document_.objects().begin(), document_.objects().end(),
                                    [selected](const AnnotationObject& object) {
                                        return object.id == *selected;
                                    });
    if (found == document_.objects().end() ||
        !std::holds_alternative<MosaicAnnotation>(found->payload))
        return;
    auto replacement = *found;
    std::get<MosaicAnnotation>(replacement.payload).blockSize = mosaicBlockSize_;
    static_cast<void>(document_.replaceObject(std::move(replacement)));
}

int AnnotationInteraction::mosaicBlockSize() const noexcept {
    return mosaicBlockSize_;
}

void AnnotationInteraction::press(QPointF point) {
    cancelDraft();
    pressedPoint_ = clip(point);
    if (tool_ != AnnotationTool::Select) {
        createDraft(pressedPoint_);
        return;
    }
    const auto hit = hitTest(pressedPoint_);
    if (!hit.has_value()) {
        document_.clearSelection();
        return;
    }
    static_cast<void>(document_.select(*hit));
    const auto found = std::find_if(document_.objects().begin(), document_.objects().end(),
                                    [hit](const AnnotationObject& object) { return object.id == *hit; });
    if (found == document_.objects().end())
        return;
    editBefore_ = *found;
    draft_ = *found;
    editMode_ = editModeFor(*found, pressedPoint_);
}

void AnnotationInteraction::move(QPointF point) {
    if (!draft_.has_value())
        return;
    if (editBefore_.has_value())
        updateEdit(clip(point));
    else
        updateDraft(clip(point));
}

void AnnotationInteraction::release(QPointF point) {
    move(point);
    if (!draft_.has_value())
        return;
    if (!editBefore_.has_value() && std::holds_alternative<FreehandAnnotation>(draft_->payload))
        appendFreehandPoint(clip(point), true);
    if (editBefore_.has_value()) {
        if (draft_->payload != editBefore_->payload)
            static_cast<void>(document_.replaceObject(*draft_));
    } else {
        const auto added = document_.addObject(draft_->payload);
        if (added.has_value())
            static_cast<void>(document_.select(*added));
    }
    cancelDraft();
}

void AnnotationInteraction::cancelDraft() noexcept {
    draft_.reset();
    editBefore_.reset();
    editMode_ = EditMode::None;
}

bool AnnotationInteraction::deleteSelection() {
    cancelDraft();
    const auto selected = document_.selectedId();
    return selected.has_value() && document_.removeObject(*selected);
}

std::optional<AnnotationId> AnnotationInteraction::hitTest(QPointF point) const {
    for (const bool mosaicLayer : {false, true}) {
        for (auto position = document_.objects().rbegin(); position != document_.objects().rend(); ++position) {
        if (std::holds_alternative<MosaicAnnotation>(position->payload) != mosaicLayer)
            continue;
        const auto hit = std::visit(
            [point](const auto& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                              std::is_same_v<Value, EllipseAnnotation> ||
                              std::is_same_v<Value, MosaicAnnotation>) {
                    return value.rect.normalized().contains(point);
                } else if constexpr (std::is_same_v<Value, ArrowAnnotation>) {
                    return pointToSegmentDistance(point, value.start, value.end) <= lineTolerance(value.style);
                } else if constexpr (std::is_same_v<Value, FreehandAnnotation>) {
                    for (std::size_t index = 1; index < value.points.size(); ++index) {
                        if (pointToSegmentDistance(point, value.points[index - 1], value.points[index]) <=
                            lineTolerance(value.style))
                            return true;
                    }
                    return false;
                } else if constexpr (std::is_same_v<Value, TextAnnotation>) {
                    return textLogicalRect(value)
                        .adjusted(-kTextHitTolerance, -kTextHitTolerance, kTextHitTolerance,
                                  kTextHitTolerance)
                        .contains(point);
                }
                return false;
            },
            position->payload);
        if (hit)
            return position->id;
        }
    }
    return std::nullopt;
}

std::optional<AnnotationObject> AnnotationInteraction::draft() const {
    return draft_;
}

bool AnnotationInteraction::hasDraft() const noexcept {
    return draft_.has_value();
}

QRectF AnnotationInteraction::bounds() const {
    const auto size = document_.snapshot().base.size();
    return {0.0, 0.0, static_cast<qreal>(size.width()), static_cast<qreal>(size.height())};
}

QPointF AnnotationInteraction::clip(QPointF point) const {
    const auto limit = bounds();
    return {std::clamp(point.x(), limit.left(), limit.right()),
            std::clamp(point.y(), limit.top(), limit.bottom())};
}

void AnnotationInteraction::createDraft(QPointF point) {
    switch (tool_) {
    case AnnotationTool::Rectangle:
        draft_ = AnnotationObject{0, RectangleAnnotation{{point, point}, style_}};
        break;
    case AnnotationTool::Ellipse:
        draft_ = AnnotationObject{0, EllipseAnnotation{{point, point}, style_}};
        break;
    case AnnotationTool::Arrow:
        draft_ = AnnotationObject{0, ArrowAnnotation{point, point, style_}};
        break;
    case AnnotationTool::Freehand:
        draft_ = AnnotationObject{0, FreehandAnnotation{{point}, style_}};
        break;
    case AnnotationTool::Mosaic:
        draft_ = AnnotationObject{0, MosaicAnnotation{{point, point}, mosaicBlockSize_}};
        break;
    default:
        break;
    }
}

void AnnotationInteraction::updateDraft(QPointF point) {
    std::visit(
        [this, point](auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                          std::is_same_v<Value, EllipseAnnotation> ||
                          std::is_same_v<Value, MosaicAnnotation>) {
                value.rect = QRectF(pressedPoint_, point).normalized().intersected(bounds());
            } else if constexpr (std::is_same_v<Value, ArrowAnnotation>) {
                value.end = point;
            } else if constexpr (std::is_same_v<Value, FreehandAnnotation>) {
                appendFreehandPoint(point, false);
            }
        },
        draft_->payload);
}

void AnnotationInteraction::updateEdit(QPointF point) {
    *draft_ = *editBefore_;
    if (editMode_ == EditMode::Move) {
        const auto sourceBounds = payloadBounds(editBefore_->payload);
        auto delta = point - pressedPoint_;
        const auto limit = bounds();
        delta.setX(std::clamp(delta.x(), limit.left() - sourceBounds.left(),
                              limit.right() - sourceBounds.right()));
        delta.setY(std::clamp(delta.y(), limit.top() - sourceBounds.top(),
                              limit.bottom() - sourceBounds.bottom()));
        translatePayload(draft_->payload, delta);
        return;
    }
    std::visit(
        [this, point](auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                          std::is_same_v<Value, EllipseAnnotation> ||
                          std::is_same_v<Value, MosaicAnnotation>) {
                const auto rect = value.rect.normalized();
                QPointF opposite;
                switch (editMode_) {
                case EditMode::ResizeTopLeft: opposite = rect.bottomRight(); break;
                case EditMode::ResizeTopRight: opposite = rect.bottomLeft(); break;
                case EditMode::ResizeBottomLeft: opposite = rect.topRight(); break;
                case EditMode::ResizeBottomRight: opposite = rect.topLeft(); break;
                default: return;
                }
                value.rect = QRectF(point, opposite).normalized().intersected(bounds());
            } else if constexpr (std::is_same_v<Value, ArrowAnnotation>) {
                if (editMode_ == EditMode::ArrowStart)
                    value.start = point;
                else if (editMode_ == EditMode::ArrowEnd)
                    value.end = point;
            }
        },
        draft_->payload);
}

void AnnotationInteraction::appendFreehandPoint(QPointF point, bool force) {
    auto& freehand = std::get<FreehandAnnotation>(draft_->payload);
    if (freehand.points.empty() ||
        (force && freehand.points.back() != point) ||
        (!force && QLineF(freehand.points.back(), point).length() >= kFreehandMergeDistance)) {
        freehand.points.push_back(point);
    }
}

AnnotationInteraction::EditMode AnnotationInteraction::editModeFor(const AnnotationObject& object,
                                                                      QPointF point) const {
    return std::visit(
        [point](const auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                          std::is_same_v<Value, EllipseAnnotation> ||
                          std::is_same_v<Value, MosaicAnnotation>) {
                const auto rect = value.rect.normalized();
                const std::array handles{
                    std::pair{EditMode::ResizeTopLeft, rect.topLeft()},
                    std::pair{EditMode::ResizeTopRight, rect.topRight()},
                    std::pair{EditMode::ResizeBottomLeft, rect.bottomLeft()},
                    std::pair{EditMode::ResizeBottomRight, rect.bottomRight()},
                };
                auto nearest = EditMode::Move;
                auto nearestDistance = kHandleTolerance;
                for (const auto& [mode, handle] : handles) {
                    const auto distance = QLineF(point, handle).length();
                    if (distance <= nearestDistance) {
                        nearest = mode;
                        nearestDistance = distance;
                    }
                }
                return nearest;
            } else if constexpr (std::is_same_v<Value, ArrowAnnotation>) {
                const auto tolerance = lineTolerance(value.style);
                const auto startDistance = QLineF(point, value.start).length();
                const auto endDistance = QLineF(point, value.end).length();
                if (startDistance > tolerance && endDistance > tolerance)
                    return EditMode::Move;
                return startDistance <= endDistance ? EditMode::ArrowStart : EditMode::ArrowEnd;
            }
            return EditMode::Move;
        },
        object.payload);
}
} // namespace lc::annotation
