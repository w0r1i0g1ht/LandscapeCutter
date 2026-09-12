#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationInteraction.hpp"
#include "annotation/AnnotationRenderer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QPainter>

namespace {
using namespace lc::annotation;

QImage coordinateImage(const QSize size) {
    QImage image(size, QImage::Format_RGB32);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x)
            image.setPixelColor(x, y, {2 * x + y, 3 * x + 2 * y, 4 * x + 3 * y});
    }
    return image;
}

AnnotationDocument mosaicDocument(const QSize size = {9, 7}) {
    return AnnotationDocument(coordinateImage(size));
}

TEST_CASE("annotation mosaic averages clipped document-origin cells with rounded RGB channels") {
    auto document = mosaicDocument();
    REQUIRE(document.addObject(MosaicAnnotation{{1, 1, 7, 5}, 4}).has_value());

    const QImage output = composeAnnotations(document.snapshot());
    CHECK(output.pixelColor(2, 2) == QColor(6, 10, 14));
    CHECK(output.pixelColor(6, 2) == QColor(13, 21, 28));
    CHECK(output.pixelColor(2, 5) == QColor(9, 15, 22));
    CHECK(output.pixelColor(6, 5) == QColor(16, 26, 36));
    CHECK(output.pixelColor(0, 0) == QColor(0, 0, 0));
}

TEST_CASE("annotation mosaic clips partial edge cells without sampling beyond its rectangle") {
    auto document = mosaicDocument();
    REQUIRE(document.addObject(MosaicAnnotation{{7, 5, 5, 5}, 4}).has_value());

    const QImage output = composeAnnotations(document.snapshot());
    CHECK(output.pixelColor(7, 5) == QColor(20, 32, 45));
    CHECK(output.pixelColor(8, 6) == QColor(22, 35, 49));
    CHECK(output.pixelColor(6, 5) == QColor(17, 28, 39));
}

TEST_CASE("annotation mosaic interaction commits creation, corner resize, and block size changes separately") {
    auto document = mosaicDocument({40, 30});
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Mosaic);
    interaction.setMosaicBlockSize(5);
    interaction.press({18, 12});
    interaction.release({4, 3});

    REQUIRE(document.objects().size() == 1);
    const auto id = document.objects().front().id;
    CHECK(std::get<MosaicAnnotation>(document.objects().front().payload) ==
          MosaicAnnotation{{4, 3, 14, 9}, 5});

    interaction.setTool(AnnotationTool::Select);
    interaction.press({18, 12});
    interaction.release({20, 15});
    CHECK(std::get<MosaicAnnotation>(document.objects().front().payload) ==
          MosaicAnnotation{{4, 3, 16, 12}, 5});
    REQUIRE(document.undo());
    CHECK(std::get<MosaicAnnotation>(document.objects().front().payload).rect == QRectF(4, 3, 14, 9));
    REQUIRE(document.redo());
    interaction.setMosaicBlockSize(9);
    CHECK(std::get<MosaicAnnotation>(document.objects().front().payload) ==
          MosaicAnnotation{{4, 3, 16, 12}, 9});
    REQUIRE(document.undo());
    CHECK(std::get<MosaicAnnotation>(document.objects().front().payload) ==
          MosaicAnnotation{{4, 3, 16, 12}, 5});
    REQUIRE(document.undo());
    CHECK(std::get<MosaicAnnotation>(document.objects().front().payload).rect == QRectF(4, 3, 14, 9));
    REQUIRE(document.undo());
    CHECK(document.objects().empty());
    REQUIRE(document.redo());
    REQUIRE(document.redo());
    CHECK(document.objects().front().id == id);
}

TEST_CASE("annotation mosaic later creations cover earlier mosaics using immutable base samples") {
    auto document = mosaicDocument({16, 12});
    REQUIRE(document.addObject(MosaicAnnotation{{0, 0, 16, 12}, 4}).has_value());
    REQUIRE(document.addObject(MosaicAnnotation{{4, 0, 6, 6}, 6}).has_value());

    const QImage output = composeAnnotations(document.snapshot());
    CHECK(output.pixelColor(5, 1) == QColor(12, 19, 26));
}

TEST_CASE("annotation mosaic safely ignores an invalid block size in a manual snapshot") {
    AnnotationSnapshot snapshot{coordinateImage({9, 7}), {{1, MosaicAnnotation{{1, 1, 4, 4}, 0}}}};
    CHECK(composeAnnotations(snapshot) == snapshot.base);
}

TEST_CASE("annotation mosaic draws below vectors and hit testing honors visual layers") {
    auto document = mosaicDocument({16, 12});
    const auto rectangle = document.addObject(RectangleAnnotation{{2, 2, 10, 7}, {Qt::red, 1}});
    const auto firstMosaic = document.addObject(MosaicAnnotation{{0, 0, 16, 12}, 4});
    const auto secondMosaic = document.addObject(MosaicAnnotation{{13, 0, 3, 4}, 2});
    REQUIRE(rectangle.has_value());
    REQUIRE(firstMosaic.has_value());
    REQUIRE(secondMosaic.has_value());

    const QImage output = composeAnnotations(document.snapshot());
    CHECK(output.pixelColor(6, 2).red() > output.pixelColor(6, 2).green());

    AnnotationInteraction interaction(document);
    CHECK(interaction.hitTest({6, 2}) == rectangle);
    CHECK(interaction.hitTest({14, 1}) == secondMosaic);
    CHECK(interaction.hitTest({1, 1}) == firstMosaic);
}

TEST_CASE("annotation mosaic split previews assemble exactly as one document render") {
    auto document = mosaicDocument({40, 12});
    REQUIRE(document.addObject(MosaicAnnotation{{5, 1, 31, 10}, 7}).has_value());
    const auto snapshot = document.snapshot();
    const QImage whole = composeAnnotations(snapshot);

    QImage left = snapshot.base.copy(0, 0, 20, 12);
    QPainter leftPainter(&left);
    drawAnnotations(leftPainter, snapshot, QTransform{}, left.rect());
    leftPainter.end();

    QImage right = snapshot.base.copy(20, 0, 20, 12);
    QPainter rightPainter(&right);
    drawAnnotations(rightPainter, snapshot, QTransform::fromTranslate(-20, 0), right.rect());
    rightPainter.end();

    QImage assembled(40, 12, QImage::Format_RGB32);
    QPainter assembledPainter(&assembled);
    assembledPainter.drawImage(0, 0, left);
    assembledPainter.drawImage(20, 0, right);
    assembledPainter.end();
    CHECK(assembled == whole);
}
} // namespace
