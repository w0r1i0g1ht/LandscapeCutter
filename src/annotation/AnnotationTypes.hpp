#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <cstdint>
#include <variant>
#include <vector>

namespace lc::annotation {
enum class AnnotationTool {
    Select,
    Rectangle,
    Ellipse,
    Arrow,
    Freehand,
    Text,
    Mosaic,
};

using AnnotationId = std::uint64_t;

struct AnnotationStyle {
    QColor color{Qt::red};
    qreal physicalSize{3.0};
    [[nodiscard]] bool operator==(const AnnotationStyle&) const = default;
};

struct RectangleAnnotation {
    QRectF rect;
    AnnotationStyle style{};
    [[nodiscard]] bool operator==(const RectangleAnnotation&) const = default;
};

struct EllipseAnnotation {
    QRectF rect;
    AnnotationStyle style{};
    [[nodiscard]] bool operator==(const EllipseAnnotation&) const = default;
};

struct ArrowAnnotation {
    QPointF start;
    QPointF end;
    AnnotationStyle style{};
    [[nodiscard]] bool operator==(const ArrowAnnotation&) const = default;
};

struct FreehandAnnotation {
    std::vector<QPointF> points;
    AnnotationStyle style{};
    [[nodiscard]] bool operator==(const FreehandAnnotation&) const = default;
};

struct TextAnnotation {
    QPointF anchor;
    QString text;
    AnnotationStyle style{Qt::red, 24.0};
    [[nodiscard]] bool operator==(const TextAnnotation&) const = default;
};

struct MosaicAnnotation {
    QRectF rect;
    int blockSize{12};
    [[nodiscard]] bool operator==(const MosaicAnnotation&) const = default;
};

using AnnotationPayload =
    std::variant<RectangleAnnotation, EllipseAnnotation, ArrowAnnotation, FreehandAnnotation,
                 TextAnnotation, MosaicAnnotation>;

struct AnnotationObject {
    AnnotationId id{};
    AnnotationPayload payload;
    [[nodiscard]] bool operator==(const AnnotationObject&) const = default;
};

struct AnnotationSnapshot {
    QImage base;
    std::vector<AnnotationObject> objects;
};
} // namespace lc::annotation
