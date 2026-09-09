#include "annotation/AnnotationRenderer.hpp"

#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>

namespace lc::annotation {
namespace {
constexpr qreal kArrowHeadAngleRadians = 28.0 * std::numbers::pi_v<qreal> / 180.0;

QPen annotationPen(const AnnotationStyle& style) {
    QPen pen(style.color, style.physicalSize);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

void drawObject(QPainter& painter, const AnnotationObject& object) {
    std::visit(
        [&painter](const auto& payload) {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, RectangleAnnotation>) {
                painter.setPen(annotationPen(payload.style));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(payload.rect);
            } else if constexpr (std::is_same_v<Payload, EllipseAnnotation>) {
                painter.setPen(annotationPen(payload.style));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(payload.rect);
            } else if constexpr (std::is_same_v<Payload, ArrowAnnotation>) {
                painter.setPen(annotationPen(payload.style));
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(payload.start, payload.end);
                const auto head = arrowHead(payload.start, payload.end, payload.style.physicalSize);
                painter.drawLine(head[0], head[1]);
                painter.drawLine(head[0], head[2]);
            } else if constexpr (std::is_same_v<Payload, FreehandAnnotation>) {
                if (payload.points.size() < 2) {
                    return;
                }
                painter.setPen(annotationPen(payload.style));
                painter.setBrush(Qt::NoBrush);
                QPainterPath path(payload.points.front());
                for (std::size_t index = 1; index < payload.points.size(); ++index) {
                    path.lineTo(payload.points[index]);
                }
                painter.drawPath(path);
            }
        },
        object.payload);
}
} // namespace

QPolygonF arrowHead(QPointF start, QPointF end, qreal width) {
    const QLineF shaft(start, end);
    const auto shaftLength = shaft.length();
    if (shaftLength <= 0.0) {
        return {end};
    }

    const auto headLength = std::min(std::max<qreal>(8.0, 4.0 * width), 0.4 * shaftLength);
    const auto direction = std::atan2(end.y() - start.y(), end.x() - start.x());
    const auto first = direction + std::numbers::pi_v<qreal> - kArrowHeadAngleRadians;
    const auto second = direction + std::numbers::pi_v<qreal> + kArrowHeadAngleRadians;
    return {end,
            {end.x() + headLength * std::cos(first), end.y() + headLength * std::sin(first)},
            {end.x() + headLength * std::cos(second), end.y() + headLength * std::sin(second)}};
}

void drawAnnotations(QPainter& painter, const AnnotationSnapshot& snapshot,
                     const QTransform& documentToTarget, const QRectF& targetClip) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setWorldTransform(QTransform{}, false);
    painter.setClipRect(targetClip, Qt::ReplaceClip);
    painter.setWorldTransform(documentToTarget, false);
    for (const auto& object : snapshot.objects) {
        drawObject(painter, object);
    }
    painter.restore();
}

QImage composeAnnotations(const AnnotationSnapshot& snapshot) {
    auto output = snapshot.base.copy();
    output.setDevicePixelRatio(1.0);
    QPainter painter(&output);
    drawAnnotations(painter, snapshot, QTransform{}, QRectF(output.rect()));
    painter.end();
    return output;
}

} // namespace lc::annotation
