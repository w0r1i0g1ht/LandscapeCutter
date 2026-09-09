#pragma once

#include "annotation/AnnotationTypes.hpp"

#include <QPolygonF>
#include <QRectF>
#include <QTransform>

class QPainter;

namespace lc::annotation {

[[nodiscard]] QPolygonF arrowHead(QPointF start, QPointF end, qreal width);

void drawAnnotations(QPainter& painter, const AnnotationSnapshot& snapshot,
                     const QTransform& documentToTarget, const QRectF& targetClip);

[[nodiscard]] QImage composeAnnotations(const AnnotationSnapshot& snapshot);

} // namespace lc::annotation
