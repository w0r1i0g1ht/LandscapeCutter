#include "annotation/AnnotationDocument.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace lc::annotation {
namespace {
[[nodiscard]] bool finite(qreal value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite(const QPointF& point) noexcept {
    return finite(point.x()) && finite(point.y());
}

[[nodiscard]] bool finite(const QRectF& rect) noexcept {
    return finite(rect.x()) && finite(rect.y()) && finite(rect.width()) && finite(rect.height());
}

[[nodiscard]] bool validStyle(const AnnotationStyle& style) noexcept {
    return style.color.isValid() && finite(style.physicalSize) && style.physicalSize > 0.0;
}

[[nodiscard]] QRectF clippedRect(const QRectF& rect, const QRectF& bounds) noexcept {
    return rect.normalized().intersected(bounds);
}

[[nodiscard]] QPointF clippedPoint(const QPointF& point, const QRectF& bounds) noexcept {
    return {std::clamp(point.x(), bounds.left(), bounds.right()),
            std::clamp(point.y(), bounds.top(), bounds.bottom())};
}

[[nodiscard]] bool isNonEmpty(const QRectF& rect) noexcept {
    return rect.width() > 0.0 && rect.height() > 0.0;
}

[[nodiscard]] bool hasDifferentPoints(const std::vector<QPointF>& points) noexcept {
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (points[index] != points.front()) {
            return true;
        }
    }
    return false;
}
} // namespace

AnnotationDocument::AnnotationDocument(QImage base)
    : base_(base.convertToFormat(QImage::Format_RGB32)) {
    base_.setDevicePixelRatio(1.0);
}

std::optional<AnnotationId> AnnotationDocument::addObject(AnnotationPayload payload) {
    const auto normalized = normalize(std::move(payload));
    if (!normalized.has_value() || nextId_ == 0) {
        return std::nullopt;
    }

    const AnnotationObject object{nextId_++, *normalized};
    const auto before = selection_;
    objects_.push_back(object);
    history_.push({std::nullopt, object, before, selection_});
    return object.id;
}

bool AnnotationDocument::replaceObject(AnnotationObject object) {
    const auto position = find(object.id);
    const auto normalized = normalize(std::move(object.payload));
    if (position == objects_.end() || !normalized.has_value()) {
        return false;
    }

    object.payload = *normalized;
    if (*position == object) {
        return false;
    }

    const auto before = *position;
    *position = object;
    history_.push({before, object, selection_, selection_});
    return true;
}

bool AnnotationDocument::removeObject(AnnotationId id) {
    const auto position = find(id);
    if (position == objects_.end()) {
        return false;
    }

    const auto before = *position;
    const auto selectionBefore = selection_;
    if (selection_ == id) {
        selection_.reset();
    }
    objects_.erase(position);
    history_.push({before, std::nullopt, selectionBefore, selection_});
    return true;
}

bool AnnotationDocument::select(AnnotationId id) noexcept {
    if (find(id) == objects_.end()) {
        return false;
    }
    selection_ = id;
    return true;
}

void AnnotationDocument::clearSelection() noexcept {
    selection_.reset();
}

std::optional<AnnotationId> AnnotationDocument::selectedId() const noexcept {
    return selection_;
}

bool AnnotationDocument::undo() {
    const auto command = history_.undo();
    if (!command.has_value()) {
        return false;
    }
    apply(command->before, command->after, command->selectionBefore);
    return true;
}

bool AnnotationDocument::redo() {
    const auto command = history_.redo();
    if (!command.has_value()) {
        return false;
    }
    apply(command->after, command->before, command->selectionAfter);
    return true;
}

bool AnnotationDocument::canUndo() const noexcept {
    return history_.canUndo();
}

bool AnnotationDocument::canRedo() const noexcept {
    return history_.canRedo();
}

const std::vector<AnnotationObject>& AnnotationDocument::objects() const noexcept {
    return objects_;
}

AnnotationSnapshot AnnotationDocument::snapshot() const {
    return {base_, objects_};
}

std::optional<AnnotationPayload> AnnotationDocument::normalize(AnnotationPayload payload) const {
    const QRectF bounds{QPointF{}, QSizeF{base_.size()}};
    if (!isNonEmpty(bounds)) {
        return std::nullopt;
    }
    return std::visit(
        [&bounds](auto value) -> std::optional<AnnotationPayload> {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, RectangleAnnotation> ||
                          std::is_same_v<Value, EllipseAnnotation>) {
                if (!finite(value.rect) || !validStyle(value.style)) {
                    return std::nullopt;
                }
                value.rect = clippedRect(value.rect, bounds);
                if (!isNonEmpty(value.rect)) {
                    return std::nullopt;
                }
                return AnnotationPayload{std::move(value)};
            } else if constexpr (std::is_same_v<Value, ArrowAnnotation>) {
                if (!finite(value.start) || !finite(value.end) || !validStyle(value.style)) {
                    return std::nullopt;
                }
                value.start = clippedPoint(value.start, bounds);
                value.end = clippedPoint(value.end, bounds);
                if (value.start == value.end) {
                    return std::nullopt;
                }
                return AnnotationPayload{std::move(value)};
            } else if constexpr (std::is_same_v<Value, FreehandAnnotation>) {
                if (!validStyle(value.style) || value.points.size() < 2 ||
                    !std::all_of(value.points.begin(), value.points.end(),
                                 [](const QPointF& point) { return finite(point); })) {
                    return std::nullopt;
                }
                for (auto& point : value.points) {
                    point = clippedPoint(point, bounds);
                }
                if (!hasDifferentPoints(value.points)) {
                    return std::nullopt;
                }
                return AnnotationPayload{std::move(value)};
            } else if constexpr (std::is_same_v<Value, TextAnnotation>) {
                if (!finite(value.anchor) || !validStyle(value.style)) {
                    return std::nullopt;
                }
                value.text.replace(QLatin1Char('\t'), QStringLiteral("    "));
                if (value.text.trimmed().isEmpty()) {
                    return std::nullopt;
                }
                value.anchor = clippedPoint(value.anchor, bounds);
                return AnnotationPayload{std::move(value)};
            } else {
                if (!finite(value.rect) || value.blockSize <= 0) {
                    return std::nullopt;
                }
                value.rect = clippedRect(value.rect, bounds);
                if (!isNonEmpty(value.rect)) {
                    return std::nullopt;
                }
                return AnnotationPayload{std::move(value)};
            }
        },
        std::move(payload));
}

std::vector<AnnotationObject>::iterator AnnotationDocument::find(AnnotationId id) noexcept {
    return std::find_if(objects_.begin(), objects_.end(),
                        [id](const AnnotationObject& object) { return object.id == id; });
}

std::vector<AnnotationObject>::const_iterator AnnotationDocument::find(AnnotationId id) const noexcept {
    return std::find_if(objects_.begin(), objects_.end(),
                        [id](const AnnotationObject& object) { return object.id == id; });
}

void AnnotationDocument::apply(const std::optional<AnnotationObject>& object,
                               const std::optional<AnnotationObject>& displacedObject,
                               std::optional<AnnotationId> selection) {
    if (object.has_value()) {
        const auto position = find(object->id);
        if (position == objects_.end()) {
            const auto insertAt = std::lower_bound(
                objects_.begin(), objects_.end(), object->id,
                [](const AnnotationObject& candidate, AnnotationId id) { return candidate.id < id; });
            objects_.insert(insertAt, *object);
        } else {
            *position = *object;
        }
    } else if (displacedObject.has_value()) {
        const auto position = find(displacedObject->id);
        if (position != objects_.end()) {
            objects_.erase(position);
        }
    }
    selection_ = selection;
}
} // namespace lc::annotation
