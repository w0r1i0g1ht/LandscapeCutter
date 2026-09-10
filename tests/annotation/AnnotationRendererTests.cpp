#include "annotation/AnnotationRenderer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QLineF>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
using namespace lc::annotation;

AnnotationSnapshot snapshotWith(std::vector<AnnotationObject> objects) {
    QImage base(64, 48, QImage::Format_RGB32);
    base.fill(Qt::white);
    return {base, std::move(objects)};
}

bool isWhite(const QColor& color) {
    return color.red() == 255 && color.green() == 255 && color.blue() == 255;
}

QImage transparentAnnotationLayer(const AnnotationSnapshot& snapshot) {
    QImage layer(snapshot.base.size(), QImage::Format_ARGB32);
    layer.fill(Qt::transparent);
    QPainter painter(&layer);
    drawAnnotations(painter, snapshot, QTransform{}, layer.rect());
    return layer;
}

QRect coveredBounds(const QImage& image) {
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    bool hasBounds = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (!isWhite(image.pixelColor(x, y))) {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
                hasBounds = true;
            }
        }
    }
    return hasBounds ? QRect(left, top, right - left + 1, bottom - top + 1) : QRect{};
}

QRect alphaBounds(const QImage& image) {
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    bool hasBounds = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() != 0) {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
                hasBounds = true;
            }
        }
    }
    return hasBounds ? QRect(left, top, right - left + 1, bottom - top + 1) : QRect{};
}

TEST_CASE("annotation arrow head follows the fixed physical-pixel formula") {
    const auto head = arrowHead({10, 20}, {50, 20}, 3.0);
    REQUIRE(head.size() == 3);
    CHECK(head.front() == QPointF(50, 20));
    CHECK(std::abs(QLineF(head.front(), head[1]).length() - 12.0) < 0.01);
    CHECK(std::abs(std::atan2(head[1].y() - 20.0, head[1].x() - 50.0)) ==
          Catch::Approx(152.0 * std::numbers::pi / 180.0).margin(0.01));

    const auto arrowSnapshot = snapshotWith({{{1, ArrowAnnotation{{10, 20}, {50, 20}, {Qt::red, 3.0}}}}});
    const auto layer = transparentAnnotationLayer(arrowSnapshot);
    const auto sideOne = (head[0] + head[1]) / 2.0;
    const auto sideTwo = (head[0] + head[2]) / 2.0;
    const auto baseQuarter = head[1] * 0.75 + head[2] * 0.25;
    CHECK(layer.pixelColor(qRound(sideOne.x()), qRound(sideOne.y())).alpha() != 0);
    CHECK(layer.pixelColor(qRound(sideTwo.x()), qRound(sideTwo.y())).alpha() != 0);
    CHECK(layer.pixelColor(qRound(baseQuarter.x()), qRound(baseQuarter.y())).alpha() < 64);
}

TEST_CASE("annotation renderer safely ignores a zero-length arrow in a manual snapshot") {
    const auto snapshot = snapshotWith({{{1, ArrowAnnotation{{24, 20}, {24, 20}, {Qt::red, 3.0}}}}});
    const auto layer = transparentAnnotationLayer(snapshot);
    CHECK(alphaBounds(layer).isEmpty());
}

TEST_CASE("annotation renderer draws fixed-image bounds, round strokes, and creation order") {
    const auto snapshot = snapshotWith({
        {1, RectangleAnnotation{{8, 8, 20, 12}, {Qt::red, 3.0}}},
        {2, EllipseAnnotation{{34, 8, 18, 16}, {Qt::blue, 3.0}}},
        {3, ArrowAnnotation{{8, 36}, {52, 36}, {Qt::green, 3.0}}},
        {4, FreehandAnnotation{{{10, 42}, {20, 38}, {30, 42}}, {Qt::black, 5.0}}},
        {5, RectangleAnnotation{{12, 12, 8, 6}, {Qt::yellow, 3.0}}},
    });

    const auto output = composeAnnotations(snapshot);
    CHECK(output.size() == QSize(64, 48));
    CHECK(output.devicePixelRatio() == 1.0);
    CHECK(output.pixelColor(12, 12) == QColor(Qt::yellow));
    CHECK(output.pixelColor(8, 8) != QColor(Qt::white));
    CHECK(output.pixelColor(43, 8) != QColor(Qt::white));
    CHECK(output.pixelColor(50, 36) != QColor(Qt::white));
    CHECK(output.pixelColor(20, 38) != QColor(Qt::white));

    const auto rectangle = composeAnnotations(snapshotWith({{{1, RectangleAnnotation{{8, 8, 20, 12}, {Qt::red, 3.0}}}}}));
    const auto ellipse = composeAnnotations(snapshotWith({{{1, EllipseAnnotation{{34, 8, 18, 16}, {Qt::blue, 3.0}}}}}));
    CHECK(coveredBounds(rectangle) == QRect(6, 6, 24, 16));
    CHECK(coveredBounds(ellipse) == QRect(32, 6, 22, 20));

    const auto freehand = transparentAnnotationLayer(snapshotWith(
        {{{1, FreehandAnnotation{{{10, 40}, {20, 30}, {30, 40}}, {Qt::black, 6.0}}}}}));
    CHECK(freehand.pixelColor(8, 40).alpha() != 0);
    CHECK(freehand.pixelColor(20, 27).alpha() != 0);
}

TEST_CASE("annotation renderer clips in target coordinates and leaves the base unchanged") {
    auto snapshot = snapshotWith({{{1, RectangleAnnotation{{4, 4, 50, 30}, {Qt::red, 3.0}}}}});
    const auto before = snapshot.base;
    QImage target(64, 48, QImage::Format_RGB32);
    target.fill(Qt::white);
    QPainter painter(&target);
    drawAnnotations(painter, snapshot, QTransform::fromTranslate(8, 6), QRectF(10, 8, 20, 12));
    painter.end();

    CHECK(isWhite(target.pixelColor(9, 12)));
    CHECK(isWhite(target.pixelColor(12, 7)));
    CHECK(isWhite(target.pixelColor(30, 12)));
    CHECK(isWhite(target.pixelColor(12, 20)));
    CHECK_FALSE(isWhite(target.pixelColor(11, 10)));
    CHECK(snapshot.base == before);
}

TEST_CASE("annotation renderer applies the target clip before rotated and noninvertible drawing") {
    const auto snapshot = snapshotWith({{{1, RectangleAnnotation{{0, 0, 28, 20}, {Qt::red, 3.0}}}}});
    QTransform transform;
    transform.translate(30, 12);
    transform.rotate(90);
    transform.shear(0.2, 0.1);
    const auto anchor = transform.map(QPointF{0, 0});
    const QRectF clip(anchor.x() - 3, anchor.y() - 3, 6, 6);

    QImage rotated(64, 48, QImage::Format_ARGB32);
    rotated.fill(Qt::transparent);
    QPainter rotatedPainter(&rotated);
    drawAnnotations(rotatedPainter, snapshot, transform, clip);
    rotatedPainter.end();
    const auto rotatedBounds = alphaBounds(rotated);
    REQUIRE_FALSE(rotatedBounds.isEmpty());
    CHECK(rotatedBounds.left() >= clip.left());
    CHECK(rotatedBounds.top() >= clip.top());
    CHECK(rotatedBounds.right() <= clip.right());
    CHECK(rotatedBounds.bottom() <= clip.bottom());

    QImage noninvertible(64, 48, QImage::Format_ARGB32);
    noninvertible.fill(Qt::transparent);
    QPainter noninvertiblePainter(&noninvertible);
    drawAnnotations(noninvertiblePainter, snapshot, QTransform(0, 0, 0, 1, 30, 12), clip);
    noninvertiblePainter.end();
    const auto noninvertibleBounds = alphaBounds(noninvertible);
    CHECK((noninvertibleBounds.isEmpty() ||
           (noninvertibleBounds.left() >= clip.left() && noninvertibleBounds.top() >= clip.top() &&
            noninvertibleBounds.right() <= clip.right() && noninvertibleBounds.bottom() <= clip.bottom())));
}

TEST_CASE("annotation renderer preview and output have equivalent mapped pixel coverage") {
    const auto snapshot = snapshotWith({
        {1, RectangleAnnotation{{8, 7, 18, 12}, {Qt::red, 3.0}}},
        {2, EllipseAnnotation{{33, 10, 15, 16}, {Qt::blue, 2.0}}},
        {3, ArrowAnnotation{{10, 35}, {48, 39}, {Qt::green, 3.0}}},
        {4, FreehandAnnotation{{{12, 42}, {20, 34}, {28, 43}}, {Qt::black, 4.0}}},
    });
    const auto output = composeAnnotations(snapshot);

    QImage preview(128, 96, QImage::Format_ARGB32);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    const QTransform documentToPreview(2, 0, 0, 2, 5, 3);
    drawAnnotations(painter, snapshot, documentToPreview, QRectF(-100, -100, 400, 300));
    painter.end();

    const auto outputBounds = coveredBounds(output);
    const auto previewBounds = alphaBounds(preview);
    const auto inverse = documentToPreview.inverted();
    const auto mappedPreviewBounds = QRectF(inverse.map(previewBounds.topLeft()),
                                            inverse.map(previewBounds.bottomRight())).normalized();
    CHECK(std::abs(outputBounds.left() - mappedPreviewBounds.left()) <= 1.0);
    CHECK(std::abs(outputBounds.top() - mappedPreviewBounds.top()) <= 1.0);
    CHECK(std::abs(outputBounds.right() - mappedPreviewBounds.right()) <= 1.0);
    CHECK(std::abs(outputBounds.bottom() - mappedPreviewBounds.bottom()) <= 1.0);

    int comparedPixels = 0;
    for (int y = outputBounds.top(); y <= outputBounds.bottom(); ++y) {
        for (int x = outputBounds.left(); x <= outputBounds.right(); ++x) {
            const auto documentColor = output.pixelColor(x, y);
            if (isWhite(documentColor)) {
                continue;
            }
            if (documentColor != QColor(Qt::red) && documentColor != QColor(Qt::blue) &&
                documentColor != QColor(Qt::green) && documentColor != QColor(Qt::black)) {
                continue;
            }
            for (int previewY = 0; previewY < 2; ++previewY) {
                for (int previewX = 0; previewX < 2; ++previewX) {
                    const auto color = preview.pixelColor(2 * x + 5 + previewX, 2 * y + 3 + previewY);
                    if (color.alpha() != 255) {
                        continue;
                    }
                    const auto overWhite = QColor(color.red(), color.green(), color.blue());
                    CHECK(std::abs(documentColor.red() - overWhite.red()) <= 2);
                    CHECK(std::abs(documentColor.green() - overWhite.green()) <= 2);
                    CHECK(std::abs(documentColor.blue() - overWhite.blue()) <= 2);
                    ++comparedPixels;
                }
            }
        }
    }
    CHECK(comparedPixels > 0);
    CHECK(inverse.isIdentity() == false);
}

TEST_CASE("annotation composition forces DPR one and never changes its base") {
    auto snapshot = snapshotWith({{{1, RectangleAnnotation{{8, 8, 20, 12}, {Qt::red, 3.0}}}}});
    snapshot.base.setDevicePixelRatio(2.0);
    const auto before = snapshot.base;
    const auto output = composeAnnotations(snapshot);
    CHECK(output.devicePixelRatio() == 1.0);
    CHECK(snapshot.base == before);
}
} // namespace
