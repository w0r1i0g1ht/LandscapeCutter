#pragma once

#include "annotation/AnnotationTypes.hpp"

#include <QPolygonF>
#include <QFont>
#include <QRectF>
#include <QTransform>

class QPainter;

namespace lc::annotation {

[[nodiscard]] QPolygonF arrowHead(QPointF start, QPointF end, qreal width);
[[nodiscard]] QFont resolvedAnnotationFont(int physicalPixelSize);
[[nodiscard]] QRectF textLogicalRect(const TextAnnotation& text);

void drawAnnotations(QPainter& painter, const AnnotationSnapshot& snapshot,
                     const QTransform& documentToTarget, const QRectF& targetClip);

[[nodiscard]] QImage composeAnnotations(const AnnotationSnapshot& snapshot);

} // namespace lc::annotation
