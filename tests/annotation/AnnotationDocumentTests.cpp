#include "annotation/AnnotationDocument.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {
using namespace lc::annotation;

QImage testImage() {
    return QImage{40, 30, QImage::Format_RGB32};
}

TEST_CASE("annotation value defaults use physical baseline properties") {
    CHECK(AnnotationStyle{}.color == QColor{Qt::red});
    CHECK(AnnotationStyle{}.physicalSize == 3.0);
    CHECK(TextAnnotation{}.style.physicalSize == 24.0);
    CHECK(MosaicAnnotation{}.blockSize == 12);
}

TEST_CASE("annotation document owns a DPR-one base and stable object ids") {
    QImage source(40, 30, QImage::Format_ARGB32);
    source.setDevicePixelRatio(2.0);
    AnnotationDocument document(source);
    const auto first = document.addObject(RectangleAnnotation{{2, 3, 10, 8}, {Qt::red, 3.0}});
    const auto second = document.addObject(EllipseAnnotation{{5, 6, 9, 7}, {Qt::blue, 2.0}});

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first != 0);
    CHECK(*second > *first);
    CHECK(*first != *second);
    CHECK(document.snapshot().base.devicePixelRatio() == 1.0);
    CHECK(document.snapshot().base.format() == QImage::Format_RGB32);
}

TEST_CASE("annotation document accepts every valid payload and clips its geometry") {
    AnnotationDocument document(testImage());

    CHECK(document.addObject(RectangleAnnotation{{-5, -4, 12, 10}, {Qt::red, 3.0}}).has_value());
    CHECK(document.addObject(EllipseAnnotation{{32, 22, 20, 20}, {Qt::blue, 2.0}}).has_value());
    CHECK(document.addObject(ArrowAnnotation{{-3, 31}, {42, -2}, {Qt::green, 3.0}}).has_value());
    CHECK(document.addObject(FreehandAnnotation{{{-2, -1}, {43, 31}}, {Qt::black, 3.0}}).has_value());
    CHECK(document.addObject(TextAnnotation{{-4, 35}, QStringLiteral("note"), {Qt::darkMagenta, 24.0}})
              .has_value());
    CHECK(document.addObject(MosaicAnnotation{{34, 25, 12, 12}, 12}).has_value());

    const auto snapshot = document.snapshot();
    REQUIRE(snapshot.objects.size() == 6);
    CHECK(std::get<RectangleAnnotation>(snapshot.objects.front().payload).rect == QRectF{0, 0, 7, 6});
    CHECK(std::get<EllipseAnnotation>(snapshot.objects[1].payload).rect == QRectF{32, 22, 8, 8});
    const auto& arrow = std::get<ArrowAnnotation>(snapshot.objects[2].payload);
    CHECK(arrow.start == QPointF{0, 30});
    CHECK(arrow.end == QPointF{40, 0});
    const auto& freehand = std::get<FreehandAnnotation>(snapshot.objects[3].payload);
    CHECK(freehand.points == std::vector<QPointF>{{0, 0}, {40, 30}});
    CHECK(std::get<TextAnnotation>(snapshot.objects[4].payload).anchor == QPointF{0, 30});
    CHECK(std::get<MosaicAnnotation>(snapshot.objects[5].payload).rect == QRectF{34, 25, 6, 5});
}

TEST_CASE("annotation document rejects invalid and empty payloads") {
    AnnotationDocument document(testImage());

    CHECK_FALSE(document.addObject(RectangleAnnotation{{1, 1, 0, 4}, {Qt::red, 3.0}}).has_value());
    CHECK_FALSE(document.addObject(ArrowAnnotation{{1, 1}, {1, 1}, {Qt::red, 3.0}}).has_value());
    CHECK_FALSE(document.addObject(FreehandAnnotation{{{1, 1}, {1, 1}}, {Qt::red, 3.0}}).has_value());
    CHECK_FALSE(document.addObject(TextAnnotation{{1, 1}, QStringLiteral(" \t\n "), {Qt::red, 24.0}})
                    .has_value());
    CHECK_FALSE(document.addObject(MosaicAnnotation{{1, 1, 4, 0}, 12}).has_value());
    CHECK_FALSE(document.addObject(RectangleAnnotation{{std::nan(""), 1, 4, 4}, {Qt::red, 3.0}})
                    .has_value());
}

TEST_CASE("removing a selected annotation clears selection without exposing it in snapshots") {
    AnnotationDocument document(testImage());
    const auto id = document.addObject(RectangleAnnotation{{2, 3, 10, 8}, {Qt::red, 3.0}}).value();

    REQUIRE(document.select(id));
    CHECK(document.snapshot().objects.size() == 1);
    CHECK(document.snapshot().objects.front().id == id);
    REQUIRE(document.removeObject(id));
    CHECK_FALSE(document.selectedId().has_value());
    CHECK_FALSE(document.select(id));
    CHECK(document.objects().empty());
}
} // namespace
