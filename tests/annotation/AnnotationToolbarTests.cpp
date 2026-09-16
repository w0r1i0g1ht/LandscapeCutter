#include "annotation/AnnotationToolbar.hpp"

#include "AnnotationTestApplication.hpp"
#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationInteraction.hpp"

#include <QDoubleSpinBox>
#include <QImage>
#include <QSpinBox>
#include <QToolButton>

#include <catch2/catch_test_macros.hpp>

namespace {
using lc::annotation::AnnotationToolbar;
using lc::annotation::AnnotationToolbarMode;
using lc::annotation::AnnotationDocument;
using lc::annotation::AnnotationInteraction;
using lc::annotation::AnnotationTool;
using lc::annotation::RectangleAnnotation;
using lc::annotation::TextAnnotation;

QImage testImage() {
    QImage image({80, 40}, QImage::Format_RGB32);
    image.fill(Qt::white);
    return image;
}

TEST_CASE("annotation toolbar exposes snip actions") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    AnnotationToolbar toolbar;

    toolbar.setMode(AnnotationToolbarMode::Snip);

    auto* copy = toolbar.findChild<QToolButton*>("copyButton");
    auto* save = toolbar.findChild<QToolButton*>("saveButton");
    auto* pin = toolbar.findChild<QToolButton*>("pinButton");
    auto* cancel = toolbar.findChild<QToolButton*>("cancelButton");
    auto* done = toolbar.findChild<QToolButton*>("doneButton");
    REQUIRE(copy != nullptr);
    REQUIRE(save != nullptr);
    REQUIRE(pin != nullptr);
    REQUIRE(cancel != nullptr);
    REQUIRE(done != nullptr);
    CHECK_FALSE(pin->isHidden());
    CHECK_FALSE(cancel->isHidden());
    CHECK(done->isHidden());
}

TEST_CASE("annotation toolbar pin mode replaces session actions with done") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    AnnotationToolbar toolbar;

    toolbar.setMode(AnnotationToolbarMode::Pin);

    auto* pin = toolbar.findChild<QToolButton*>("pinButton");
    auto* cancel = toolbar.findChild<QToolButton*>("cancelButton");
    auto* done = toolbar.findChild<QToolButton*>("doneButton");
    REQUIRE(pin != nullptr);
    REQUIRE(cancel != nullptr);
    REQUIRE(done != nullptr);
    CHECK(pin->isHidden());
    CHECK(cancel->isHidden());
    CHECK_FALSE(done->isHidden());
}

TEST_CASE("annotation toolbar reflects the active tool and its properties") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    AnnotationDocument document(testImage());
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Rectangle);
    interaction.setStyle({Qt::blue, 7.0});
    AnnotationToolbar toolbar;

    toolbar.setContentAvailable(true);
    toolbar.setContext(&document, &interaction);

    auto* rectangle = toolbar.findChild<QToolButton*>("rectangleToolButton");
    auto* lineWidth = toolbar.findChild<QDoubleSpinBox*>("lineWidthSpinBox");
    auto* blockSize = toolbar.findChild<QSpinBox*>("mosaicBlockSizeSpinBox");
    REQUIRE(rectangle != nullptr);
    REQUIRE(lineWidth != nullptr);
    REQUIRE(blockSize != nullptr);
    CHECK(rectangle->isChecked());
    CHECK_FALSE(lineWidth->isHidden());
    CHECK(lineWidth->value() == 7.0);
    CHECK(blockSize->isHidden());
}

TEST_CASE("annotation toolbar reads the selected object's real style") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    AnnotationDocument document(testImage());
    const auto id =
        document.addObject(RectangleAnnotation{{5, 6, 20, 10}, {Qt::green, 9.0}});
    REQUIRE(id.has_value());
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Select);
    interaction.setStyle({Qt::red, 3.0});
    REQUIRE(document.select(*id));
    AnnotationToolbar toolbar;

    toolbar.setContentAvailable(true);
    toolbar.setContext(&document, &interaction);

    auto* lineWidth = toolbar.findChild<QDoubleSpinBox*>("lineWidthSpinBox");
    REQUIRE(lineWidth != nullptr);
    CHECK(lineWidth->value() == 9.0);
}

TEST_CASE("annotation toolbar exposes font size for new and selected text") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    AnnotationDocument document(testImage());
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Text);
    AnnotationToolbar toolbar;

    toolbar.setContentAvailable(true);
    toolbar.setContext(&document, &interaction);

    auto* fontSize = toolbar.findChild<QSpinBox*>("fontSizeSpinBox");
    REQUIRE(fontSize != nullptr);
    CHECK_FALSE(fontSize->isHidden());
    CHECK(fontSize->value() == 24);

    const auto id = document.addObject(TextAnnotation{{5, 6}, QStringLiteral("text"),
                                                       {Qt::green, 18.0}});
    REQUIRE(id.has_value());
    REQUIRE(document.select(*id));
    interaction.setTool(AnnotationTool::Select);
    toolbar.refresh();
    CHECK_FALSE(fontSize->isHidden());
    CHECK(fontSize->value() == 18);

    fontSize->setValue(42);
    CHECK(std::get<TextAnnotation>(document.objects().front().payload).style.physicalSize == 42.0);
}

TEST_CASE("annotation toolbar emits host actions once and disables output while busy") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    AnnotationToolbar toolbar;
    toolbar.setContentAvailable(true);
    int copies{};
    int pins{};
    QObject::connect(&toolbar, &AnnotationToolbar::copyRequested, [&copies] { ++copies; });
    QObject::connect(&toolbar, &AnnotationToolbar::pinRequested, [&pins] { ++pins; });
    auto* copy = toolbar.findChild<QToolButton*>("copyButton");
    auto* pin = toolbar.findChild<QToolButton*>("pinButton");
    REQUIRE(copy != nullptr);
    REQUIRE(pin != nullptr);

    copy->click();
    pin->click();
    CHECK(copies == 1);
    CHECK(pins == 1);

    toolbar.setBusy(true);
    CHECK_FALSE(copy->isEnabled());
    CHECK_FALSE(pin->isEnabled());
}
} // namespace
