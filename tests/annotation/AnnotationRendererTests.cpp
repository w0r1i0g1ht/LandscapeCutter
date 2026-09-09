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
}

TEST_CASE("annotation renderer draws fixed-image geometry in creation order") {
    const auto snapshot = snapshotWith({
        {1, RectangleAnnotation{{8, 8, 20, 12}, {Qt::red, 3.0}}},
        {2, EllipseAnnotation{{34, 8, 18, 16}, {Qt::blue, 3.0}}},
        {3, ArrowAnnotation{{8, 36}, {52, 36}, {Qt::green, 3.0}}},
        {4, FreehandAnnotation{{{10, 42}, {20, 38}, {30, 42}}, {Qt::black, 5.0}}},
        {5, RectangleAnnotation{{12, 12, 8, 6}, {Qt::yellow, 3.0}}},
    });

    const auto output = composeAnnotations(snapshot);
    CHECK(output.size() == QSize(64, 48));
    CHECK(output.pixelColor(12, 12) == QColor(Qt::yellow));
    CHECK(output.pixelColor(8, 8) != QColor(Qt::white));
    CHECK(output.pixelColor(43, 8) != QColor(Qt::white));
    CHECK(output.pixelColor(50, 36) != QColor(Qt::white));
    CHECK(output.pixelColor(20, 38) != QColor(Qt::white));
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

TEST_CASE("annotation renderer preview and output have equivalent mapped pixel coverage") {
    const auto snapshot = snapshotWith({
        {1, RectangleAnnotation{{8, 7, 18, 12}, {Qt::red, 3.0}}},
        {2, EllipseAnnotation{{33, 10, 15, 16}, {Qt::blue, 2.0}}},
        {3, ArrowAnnotation{{10, 35}, {48, 39}, {Qt::green, 3.0}}},
        {4, FreehandAnnotation{{{12, 42}, {20, 34}, {28, 43}}, {Qt::black, 4.0}}},
    });
    const auto output = composeAnnotations(snapshot);

    QImage preview(128, 96, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    const QTransform documentToPreview = QTransform::fromTranslate(5, 3) * QTransform::fromScale(2, 2);
    drawAnnotations(painter, snapshot, documentToPreview, preview.rect());
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

    for (const auto& point : {QPointF{8, 7}, QPointF{33, 10}, QPointF{48, 39}}) {
        const auto documentColor = output.pixelColor(qRound(point.x()), qRound(point.y()));
        const auto previewPoint = documentToPreview.map(point);
        const auto previewColor = preview.pixelColor(qRound(previewPoint.x()), qRound(previewPoint.y()));
        const auto overWhite = [alpha = previewColor.alpha()](int channel) {
            return (channel * alpha + 255 * (255 - alpha)) / 255;
        };
        CHECK(std::abs(documentColor.red() - overWhite(previewColor.red())) <= 2);
        CHECK(std::abs(documentColor.green() - overWhite(previewColor.green())) <= 2);
        CHECK(std::abs(documentColor.blue() - overWhite(previewColor.blue())) <= 2);
    }
    CHECK(inverse.isIdentity() == false);
}
} // namespace
