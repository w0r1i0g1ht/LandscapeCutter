#include "annotation/AnnotationRenderer.hpp"

#include <QLineF>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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

void drawText(QPainter& painter, const TextAnnotation& text) {
    const QFont font = resolvedAnnotationFont(qMax(1, qRound(text.style.physicalSize)));
    const QFontMetricsF metrics(font);
    painter.setPen(QPen(text.style.color));
    painter.setFont(font);
    const QStringList lines = text.text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (qsizetype index = 0; index < lines.size(); ++index) {
        painter.drawText(QPointF{text.anchor.x(), text.anchor.y() + metrics.ascent() +
                                             index * metrics.lineSpacing()},
                         lines[index]);
    }
}

void drawMosaic(QPainter& painter, const MosaicAnnotation& mosaic, const QImage& base) {
    if (mosaic.blockSize <= 0 || base.isNull())
        return;
    const QRectF region = mosaic.rect.normalized().intersected(QRectF(base.rect()));
    if (region.isEmpty())
        return;

    const std::int64_t blockSize = mosaic.blockSize;
    const std::int64_t firstX =
        static_cast<std::int64_t>(std::floor(region.left() / static_cast<qreal>(blockSize))) * blockSize;
    const std::int64_t firstY =
        static_cast<std::int64_t>(std::floor(region.top() / static_cast<qreal>(blockSize))) * blockSize;
    const std::int64_t lastX = static_cast<std::int64_t>(std::ceil(region.right()));
    const std::int64_t lastY = static_cast<std::int64_t>(std::ceil(region.bottom()));
    painter.setPen(Qt::NoPen);
    for (std::int64_t cellY = firstY; cellY < lastY;) {
        for (std::int64_t cellX = firstX; cellX < lastX;) {
            const QRectF cell{static_cast<qreal>(cellX), static_cast<qreal>(cellY),
                              static_cast<qreal>(blockSize), static_cast<qreal>(blockSize)};
            const QRectF intersection = cell.intersected(region);
            const int left = std::max(0, qCeil(intersection.left()));
            const int top = std::max(0, qCeil(intersection.top()));
            const int right = std::min(base.width(), qCeil(intersection.right()));
            const int bottom = std::min(base.height(), qCeil(intersection.bottom()));
            if (left < right && top < bottom) {
                std::int64_t red{};
                std::int64_t green{};
                std::int64_t blue{};
                const std::int64_t count =
                    static_cast<std::int64_t>(right - left) * (bottom - top);
                for (int y = top; y < bottom; ++y) {
                    for (int x = left; x < right; ++x) {
                        const QColor color = base.pixelColor(x, y);
                        red += color.red();
                        green += color.green();
                        blue += color.blue();
                    }
                }
                const QColor average{static_cast<int>((red + count / 2) / count),
                                     static_cast<int>((green + count / 2) / count),
                                     static_cast<int>((blue + count / 2) / count)};
                painter.setBrush(average);
                painter.drawRect(QRectF(left, top, right - left, bottom - top));
            }

            if (cellX > std::numeric_limits<std::int64_t>::max() - blockSize)
                break;
            cellX += blockSize;
        }
        if (cellY > std::numeric_limits<std::int64_t>::max() - blockSize)
            break;
        cellY += blockSize;
    }
}

void drawObject(QPainter& painter, const AnnotationObject& object, const QImage& base) {
    std::visit(
        [&painter, &base](const auto& payload) {
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
                const auto head = arrowHead(payload.start, payload.end, payload.style.physicalSize);
                if (head.size() == 3) {
                    painter.drawLine(payload.start, payload.end);
                    painter.drawLine(head[0], head[1]);
                    painter.drawLine(head[0], head[2]);
                }
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
            } else if constexpr (std::is_same_v<Payload, TextAnnotation>) {
                drawText(painter, payload);
            } else if constexpr (std::is_same_v<Payload, MosaicAnnotation>) {
                drawMosaic(painter, payload, base);
            }
        },
        object.payload);
}
} // namespace

QFont resolvedAnnotationFont(int physicalPixelSize) {
    const QStringList candidates{QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI"),
                                 QStringLiteral("Arial")};
    const QStringList available = QFontDatabase::families();
    QFont font;
    const auto selected = std::find_if(candidates.begin(), candidates.end(),
                                       [&available](const QString& candidate) {
                                           return available.contains(candidate, Qt::CaseInsensitive);
                                       });
    if (selected != candidates.end())
        font.setFamily(*selected);
    else
        font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPixelSize(std::max(1, physicalPixelSize));
    font.setWeight(QFont::Normal);
    font.setItalic(false);
    return font;
}

int transformedTextPixelSize(const qreal physicalPixelSize,
                             const QTransform& documentToTarget) noexcept {
    const auto verticalScale = std::hypot(documentToTarget.m21(), documentToTarget.m22());
    const auto transformed = physicalPixelSize * verticalScale;
    if (!std::isfinite(transformed) || transformed <= 0.0)
        return 1;
    const auto maximum = static_cast<qreal>(std::numeric_limits<int>::max());
    return std::max(1, static_cast<int>(std::llround(std::min(transformed, maximum))));
}

QRectF textLogicalRect(const TextAnnotation& text) {
    const QFontMetricsF metrics(resolvedAnnotationFont(qMax(1, qRound(text.style.physicalSize))));
    const QStringList lines = text.text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    qreal width{};
    for (const QString& line : lines)
        width = std::max(width, metrics.horizontalAdvance(line));
    return {text.anchor, QSizeF{width, metrics.lineSpacing() * std::max<qsizetype>(1, lines.size())}};
}

QPolygonF arrowHead(QPointF start, QPointF end, qreal width) {
    const QLineF shaft(start, end);
    const auto shaftLength = shaft.length();
    if (shaftLength <= 0.0) {
        return {};
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
    for (const auto& object : snapshot.objects)
        if (std::holds_alternative<MosaicAnnotation>(object.payload))
            drawObject(painter, object, snapshot.base);
    for (const auto& object : snapshot.objects)
        if (!std::holds_alternative<MosaicAnnotation>(object.payload))
            drawObject(painter, object, snapshot.base);
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
