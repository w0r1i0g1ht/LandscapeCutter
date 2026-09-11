#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationRenderer.hpp"
#include "AnnotationTestApplication.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {
using namespace lc::annotation;

AnnotationSnapshot textSnapshot(const TextAnnotation& text) {
    QImage base(160, 100, QImage::Format_RGB32);
    base.fill(Qt::white);
    return {base, {{1, text}}};
}

QRect nonWhiteBounds(const QImage& image) {
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) != QColor(Qt::white)) {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
        }
    }
    return right < left ? QRect{} : QRect(left, top, right - left + 1, bottom - top + 1);
}

bool textPixelsWithinTolerance(const QImage& actual, const QImage& expected,
                               const QRectF& logicalBounds) {
    const QRect sampled = logicalBounds.toAlignedRect().intersected(actual.rect()).intersected(expected.rect());
    int channels{};
    int withinTolerance{};
    for (int y = sampled.top(); y <= sampled.bottom(); ++y) {
        for (int x = sampled.left(); x <= sampled.right(); ++x) {
            const QColor rendered = actual.pixelColor(x, y);
            const QColor reference = expected.pixelColor(x, y);
            if (rendered == QColor(Qt::white) && reference == QColor(Qt::white))
                continue;
            for (const int difference : {std::abs(rendered.red() - reference.red()),
                                         std::abs(rendered.green() - reference.green()),
                                         std::abs(rendered.blue() - reference.blue())}) {
                ++channels;
                withinTolerance += difference <= 16 ? 1 : 0;
            }
        }
    }
    return channels > 0 && withinTolerance * 100 >= channels * 95;
}

TEST_CASE("annotation text normalization expands tabs and rejects whitespace-only payloads") {
    static_cast<void>(annotationTestApplication());
    QImage base(80, 60, QImage::Format_RGB32);
    AnnotationDocument document(base);

    const auto id = document.addObject(TextAnnotation{{4, 5}, QStringLiteral("A\tB"), {Qt::red, 24}});
    REQUIRE(id.has_value());
    CHECK(std::get<TextAnnotation>(document.objects().front().payload).text == QStringLiteral("A    B"));
    CHECK_FALSE(document.addObject(TextAnnotation{{4, 5}, QStringLiteral(" \t\n "), {Qt::red, 24}})
                    .has_value());
}

TEST_CASE("annotation text logical bounds use physical font metrics and line spacing") {
    static_cast<void>(annotationTestApplication());
    const TextAnnotation text{{12, 18}, QStringLiteral("Wide\nI"), {Qt::blue, 24}};
    const QFont font = resolvedAnnotationFont(24);
    const QFontMetricsF metrics(font);
    const QRectF logical = textLogicalRect(text);

    CHECK(font.pixelSize() == 24);
    CHECK(font.weight() == QFont::Normal);
    CHECK_FALSE(font.italic());
    CHECK(logical.topLeft() == text.anchor);
    CHECK(logical.height() == metrics.lineSpacing() * 2.0);
    CHECK(logical.width() == metrics.horizontalAdvance(QStringLiteral("Wide")));
}

TEST_CASE("annotation text font resolves through the required fallback chain") {
    static_cast<void>(annotationTestApplication());
    const QFont font = resolvedAnnotationFont(19);
    const QStringList candidates{QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI"),
                                 QStringLiteral("Arial"), QFontDatabase::systemFont(QFontDatabase::GeneralFont).family()};

    CHECK(candidates.contains(font.family()));
    CHECK(font.pixelSize() == 19);
    CHECK(font.weight() == QFont::Normal);
    CHECK_FALSE(font.italic());
}

TEST_CASE("annotation text output starts at its baseline and matches fixed physical pixels") {
    static_cast<void>(annotationTestApplication());
    const TextAnnotation text{{16, 18}, QStringLiteral("Hi\nQt"), {Qt::red, 24}};
    const auto snapshot = textSnapshot(text);
    const QImage actual = composeAnnotations(snapshot);
    QImage expected = snapshot.base.copy();
    QPainter painter(&expected);
    painter.setPen(Qt::red);
    painter.setFont(resolvedAnnotationFont(24));
    const QFontMetricsF metrics(painter.font());
    painter.drawText(QPointF{16, 18 + metrics.ascent()}, QStringLiteral("Hi"));
    painter.drawText(QPointF{16, 18 + metrics.ascent() + metrics.lineSpacing()}, QStringLiteral("Qt"));
    painter.end();

    const QRect actualBounds = nonWhiteBounds(actual);
    const QRect expectedBounds = nonWhiteBounds(expected);
    REQUIRE_FALSE(actualBounds.isEmpty());
    CHECK(std::abs(actualBounds.left() - expectedBounds.left()) <= 1);
    CHECK(std::abs(actualBounds.top() - expectedBounds.top()) <= 1);
    CHECK(std::abs(actualBounds.right() - expectedBounds.right()) <= 1);
    CHECK(std::abs(actualBounds.bottom() - expectedBounds.bottom()) <= 1);

    CHECK(textPixelsWithinTolerance(actual, expected, textLogicalRect(text)));
    const TextAnnotation missing{text.anchor, QString{}, text.style};
    CHECK_FALSE(textPixelsWithinTolerance(composeAnnotations(textSnapshot(missing)), expected,
                                          textLogicalRect(text)));
}
} // namespace
