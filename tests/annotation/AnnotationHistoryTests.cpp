#include "annotation/AnnotationDocument.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {
using namespace lc::annotation;

QImage testImage() {
    return QImage{40, 30, QImage::Format_RGB32};
}

RectangleAnnotation testRectangle() {
    return {{2, 3, 10, 8}, {Qt::red, 3.0}};
}

EllipseAnnotation testEllipse() {
    return {{5, 6, 9, 7}, {Qt::blue, 2.0}};
}

TEST_CASE("annotation history restores add update and delete values with selection") {
    AnnotationDocument document(testImage());
    const auto id = document.addObject(testRectangle()).value();
    REQUIRE(document.select(id));

    auto replacement = document.objects().front();
    replacement.payload = testEllipse();
    REQUIRE(document.replaceObject(replacement));
    REQUIRE(document.removeObject(id));
    CHECK(document.objects().empty());

    REQUIRE(document.undo());
    REQUIRE(document.objects().size() == 1);
    CHECK(document.selectedId() == id);
    REQUIRE(document.undo());
    CHECK(std::holds_alternative<RectangleAnnotation>(document.objects().front().payload));
    REQUIRE(document.undo());
    CHECK(document.objects().empty());

    REQUIRE(document.redo());
    REQUIRE(document.objects().size() == 1);
    CHECK(document.objects().front().id == id);
    CHECK(std::get<RectangleAnnotation>(document.objects().front().payload) == testRectangle());
    CHECK_FALSE(document.selectedId().has_value());
    REQUIRE(document.redo());
    REQUIRE(document.objects().size() == 1);
    CHECK(document.objects().front().id == id);
    CHECK(std::get<EllipseAnnotation>(document.objects().front().payload) == testEllipse());
    CHECK(document.selectedId() == id);
    REQUIRE(document.redo());
    CHECK(document.objects().empty());
    CHECK_FALSE(document.selectedId().has_value());
}

TEST_CASE("annotation history drops redo after a new object command") {
    AnnotationDocument document(testImage());
    const auto id = document.addObject(testRectangle()).value();
    REQUIRE(document.undo());
    REQUIRE(document.addObject(testEllipse()).has_value());
    CHECK_FALSE(document.canRedo());
    CHECK(document.objects().size() == 1);
    CHECK(document.objects().front().id != id);
}

TEST_CASE("annotation history does not record unchanged replacement values") {
    AnnotationDocument document(testImage());
    REQUIRE(document.addObject(testRectangle()).has_value());
    const auto unchanged = document.objects().front();

    CHECK_FALSE(document.replaceObject(unchanged));
    REQUIRE(document.undo());
    CHECK(document.objects().empty());
}

TEST_CASE("annotation history retains only the newest one hundred commands") {
    AnnotationDocument document(testImage());
    for (int index = 0; index < 101; ++index) {
        REQUIRE(document.addObject(RectangleAnnotation{{static_cast<qreal>(index % 39), 0, 1, 1},
                                                       {Qt::red, 3.0}})
                    .has_value());
    }

    for (int index = 0; index < 100; ++index) {
        REQUIRE(document.undo());
    }
    CHECK_FALSE(document.canUndo());
    CHECK(document.objects().size() == 1);
}
} // namespace
