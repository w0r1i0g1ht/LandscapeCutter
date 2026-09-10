#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationInteraction.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {
using namespace lc::annotation;

AnnotationDocument document() {
    QImage base(40, 30, QImage::Format_RGB32);
    base.fill(Qt::white);
    return AnnotationDocument(std::move(base));
}

TEST_CASE("annotation interaction normalizes and clips a reverse rectangle drag") {
    auto value = document();
    AnnotationInteraction interaction(value);
    interaction.setTool(AnnotationTool::Rectangle);

    interaction.press({35, 25});
    interaction.move({-4, -3});
    CHECK(value.objects().empty());
    REQUIRE(interaction.draft().has_value());
    CHECK(std::get<RectangleAnnotation>(interaction.draft()->payload).rect == QRectF(0, 0, 35, 25));
    interaction.release({-4, -3});

    REQUIRE(value.objects().size() == 1);
    CHECK(std::get<RectangleAnnotation>(value.objects().front().payload).rect == QRectF(0, 0, 35, 25));
}

TEST_CASE("annotation interaction rejects zero gestures and preserves arrow direction") {
    auto value = document();
    AnnotationInteraction interaction(value);
    interaction.setTool(AnnotationTool::Arrow);
    interaction.press({6, 7});
    interaction.release({6, 7});
    CHECK(value.objects().empty());

    interaction.press({32, 24});
    interaction.release({4, 3});
    REQUIRE(value.objects().size() == 1);
    const auto& arrow = std::get<ArrowAnnotation>(value.objects().front().payload);
    CHECK(arrow.start == QPointF(32, 24));
    CHECK(arrow.end == QPointF(4, 3));
}

TEST_CASE("annotation interaction merges nearby freehand moves and retains a distinct release point") {
    auto value = document();
    AnnotationInteraction interaction(value);
    interaction.setTool(AnnotationTool::Freehand);
    interaction.press({1, 1});
    interaction.move({2, 1});
    interaction.move({2.49, 1});
    interaction.move({3, 1});
    interaction.release({4, 1});

    REQUIRE(value.objects().size() == 1);
    const auto& points = std::get<FreehandAnnotation>(value.objects().front().payload).points;
    REQUIRE(points.size() == 3);
    CHECK(points[0] == QPointF(1, 1));
    CHECK(points[1] == QPointF(3, 1));
    CHECK(points[2] == QPointF(4, 1));
}

TEST_CASE("annotation interaction commits one replacement for a completed move") {
    auto value = document();
    const auto id = value.addObject(RectangleAnnotation{{5, 5, 12, 10}, {Qt::red, 3}});
    REQUIRE(id.has_value());
    AnnotationInteraction interaction(value);
    interaction.setTool(AnnotationTool::Select);
    interaction.press({11, 10});
    interaction.move({14, 12});
    interaction.move({17, 13});
    CHECK(std::get<RectangleAnnotation>(value.objects().front().payload).rect == QRectF(5, 5, 12, 10));
    interaction.release({17, 13});

    CHECK(std::get<RectangleAnnotation>(value.objects().front().payload).rect == QRectF(11, 8, 12, 10));
    REQUIRE(value.undo());
    CHECK(std::get<RectangleAnnotation>(value.objects().front().payload).rect == QRectF(5, 5, 12, 10));
    CHECK(value.canUndo());
    REQUIRE(value.undo());
    CHECK(value.objects().empty());
}

TEST_CASE("annotation interaction edits rectangle corners and arrow endpoints") {
    auto value = document();
    const auto rectangle = value.addObject(RectangleAnnotation{{5, 5, 10, 8}, {Qt::red, 3}});
    const auto arrow = value.addObject(ArrowAnnotation{{20, 5}, {30, 10}, {Qt::blue, 3}});
    REQUIRE(rectangle.has_value());
    REQUIRE(arrow.has_value());
    AnnotationInteraction interaction(value);
    interaction.setTool(AnnotationTool::Select);

    interaction.press({5, 5});
    interaction.release({2, 3});
    CHECK(std::get<RectangleAnnotation>(value.objects()[0].payload).rect == QRectF(2, 3, 13, 10));

    interaction.press({30, 10});
    interaction.release({38, 22});
    const auto& edited = std::get<ArrowAnnotation>(value.objects()[1].payload);
    CHECK(edited.start == QPointF(20, 5));
    CHECK(edited.end == QPointF(38, 22));
}

TEST_CASE("annotation interaction hits the topmost box and line tolerance then moves freehand") {
    auto value = document();
    const auto rectangle = value.addObject(RectangleAnnotation{{3, 3, 20, 14}, {Qt::red, 3}});
    const auto ellipse = value.addObject(EllipseAnnotation{{8, 6, 16, 12}, {Qt::blue, 3}});
    const auto arrow = value.addObject(ArrowAnnotation{{2, 26}, {32, 26}, {Qt::green, 8}});
    const auto brush = value.addObject(FreehandAnnotation{{{5, 20}, {10, 22}, {16, 20}}, {Qt::black, 3}});
    REQUIRE(rectangle.has_value());
    REQUIRE(ellipse.has_value());
    REQUIRE(arrow.has_value());
    REQUIRE(brush.has_value());
    AnnotationInteraction interaction(value);

    CHECK(interaction.hitTest({10, 10}) == ellipse);
    CHECK(interaction.hitTest({18, 30}) == arrow);
    CHECK(interaction.hitTest({28, 26}) == arrow);
    CHECK(interaction.hitTest({10, 24}) == brush);

    interaction.setTool(AnnotationTool::Select);
    interaction.press({10, 21});
    interaction.release({15, 24});
    const auto& points = std::get<FreehandAnnotation>(value.objects()[3].payload).points;
    CHECK(points[0] == QPointF(10, 23));
    CHECK(points[2] == QPointF(21, 23));
}

TEST_CASE("annotation interaction applies selected style and delete as individual history commands") {
    auto value = document();
    const auto id = value.addObject(EllipseAnnotation{{5, 5, 12, 9}, {Qt::red, 3}});
    REQUIRE(id.has_value());
    AnnotationInteraction interaction(value);
    interaction.setTool(AnnotationTool::Select);
    interaction.press({10, 9});
    interaction.release({10, 9});
    interaction.setStyle({Qt::blue, 7});
    CHECK(std::get<EllipseAnnotation>(value.objects().front().payload).style == AnnotationStyle{Qt::blue, 7});
    REQUIRE(value.undo());
    CHECK(std::get<EllipseAnnotation>(value.objects().front().payload).style == AnnotationStyle{Qt::red, 3});
    REQUIRE(value.redo());
    REQUIRE(interaction.deleteSelection());
    CHECK(value.objects().empty());
    REQUIRE(value.undo());
    CHECK(value.objects().size() == 1);
}

TEST_CASE("annotation interaction cancel draft restores the committed document") {
    auto value = document();
    AnnotationInteraction interaction(value);
    interaction.setTool(AnnotationTool::Ellipse);
    interaction.press({3, 4});
    interaction.move({20, 18});
    REQUIRE(interaction.draft().has_value());
    interaction.cancelDraft();
    CHECK_FALSE(interaction.draft().has_value());
    CHECK(value.objects().empty());
}
} // namespace
