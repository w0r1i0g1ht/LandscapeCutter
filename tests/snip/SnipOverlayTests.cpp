#include "snip/SnipOverlay.hpp"
#include "annotation/AnnotationInteraction.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>

#include <catch2/catch_test_macros.hpp>

namespace {
using lc::snip::FrozenMonitor;
using lc::snip::SelectionModel;
using lc::snip::SnipOverlay;
using namespace lc::annotation;

class ApplicationFixture {
  private:
    int argc{1};
    char applicationName[18]{"snip-overlay-test"};
    char* argv[2]{applicationName, nullptr};

  public:
    ApplicationFixture() : application(argc, argv) {}

    QApplication application;
};

FrozenMonitor frozenMonitor() {
    QImage image{QSize{40, 30}, QImage::Format_RGB32};
    image.fill(Qt::red);
    return {{-40, 10, 40, 30}, image};
}

SelectionModel selectionFor(const QRect& bounds) {
    SelectionModel model;
    model.setBounds(bounds);
    model.press({-30, 15}, 2);
    model.move({-10, 35});
    model.release();
    return model;
}

AnnotationDocument annotationDocument(const QSize& size = {40, 30}) {
    QImage base(size, QImage::Format_RGB32);
    base.fill(Qt::white);
    return AnnotationDocument(std::move(base));
}

TEST_CASE("overlay presents the frozen monitor and shared selection controls") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};

    CHECK(overlay.windowFlags().testFlag(Qt::FramelessWindowHint));
    CHECK(overlay.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    CHECK(overlay.size() == QSize{40, 30});

    auto* copy = overlay.findChild<QToolButton*>("copyButton");
    auto* save = overlay.findChild<QToolButton*>("saveButton");
    auto* cancel = overlay.findChild<QToolButton*>("cancelButton");
    REQUIRE(copy != nullptr);
    REQUIRE(save != nullptr);
    REQUIRE(cancel != nullptr);
    CHECK(copy->isEnabled());
    CHECK(save->isEnabled());
    CHECK(cancel->isEnabled());

    overlay.setBusy(true);
    CHECK_FALSE(copy->isEnabled());
    CHECK_FALSE(save->isEnabled());
    CHECK(cancel->isEnabled());
    overlay.setBusy(false);
    CHECK(copy->isEnabled());
    CHECK(save->isEnabled());

    overlay.refresh();
}

TEST_CASE("overlay keyboard shortcuts signal shared selection actions") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};
    int copies{};
    int saves{};
    int cancellations{};
    QObject::connect(&overlay, &SnipOverlay::copyRequested, [&copies] { ++copies; });
    QObject::connect(&overlay, &SnipOverlay::saveRequested, [&saves] { ++saves; });
    QObject::connect(&overlay, &SnipOverlay::cancelRequested,
                     [&cancellations] { ++cancellations; });

    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &enter);
    QKeyEvent copy{QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier};
    QApplication::sendEvent(&overlay, &copy);
    QKeyEvent save{QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier};
    QApplication::sendEvent(&overlay, &save);
    QKeyEvent escape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &escape);

    CHECK(copies == 2);
    CHECK(saves == 1);
    CHECK(cancellations == 1);
}

TEST_CASE("overlay shortcuts do not copy or save during an empty selection") {
    ApplicationFixture fixture;
    SelectionModel model;
    model.setBounds({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};
    int copies{};
    int saves{};
    QObject::connect(&overlay, &SnipOverlay::copyRequested, [&copies] { ++copies; });
    QObject::connect(&overlay, &SnipOverlay::saveRequested, [&saves] { ++saves; });

    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &enter);
    QKeyEvent save{QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier};
    QApplication::sendEvent(&overlay, &save);

    CHECK(copies == 0);
    CHECK(saves == 0);
}

TEST_CASE("a busy overlay does not change the shared selection through mouse input") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    const auto original = model.rect();
    SnipOverlay overlay{frozenMonitor(), model};
    overlay.setBusy(true);

    QMouseEvent press{QEvent::MouseButtonPress, QPointF{10.0, 10.0}, QPointF{10.0, 10.0},
                      Qt::LeftButton,           Qt::LeftButton,      Qt::NoModifier};
    QMouseEvent move{QEvent::MouseMove, QPointF{30.0, 20.0}, QPointF{30.0, 20.0},
                     Qt::NoButton,      Qt::LeftButton,      Qt::NoModifier};
    QMouseEvent doubleClick{QEvent::MouseButtonDblClick,
                            QPointF{10.0, 10.0},
                            QPointF{10.0, 10.0},
                            Qt::LeftButton,
                            Qt::LeftButton,
                            Qt::NoModifier};
    QApplication::sendEvent(&overlay, &press);
    QApplication::sendEvent(&overlay, &move);
    QApplication::sendEvent(&overlay, &doubleClick);

    CHECK(model.rect() == original);
}

TEST_CASE("an overlay outside the selection does not show a duplicate toolbar") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 80, 30});
    QImage image{QSize{40, 30}, QImage::Format_RGB32};
    image.fill(Qt::blue);
    SnipOverlay overlay{{{0, 10, 40, 30}, image}, model};

    auto* toolbar = overlay.findChild<QWidget*>("snipToolbar");
    REQUIRE(toolbar != nullptr);
    CHECK(toolbar->isHidden());
}

TEST_CASE("closing an overlay requests cancellation") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};
    int cancellations{};
    QObject::connect(&overlay, &SnipOverlay::cancelRequested,
                     [&cancellations] { ++cancellations; });

    overlay.close();

    CHECK(cancellations == 1);
}

TEST_CASE("annotation overlay routes a drag to shared interaction instead of selection") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Rectangle);
    SnipOverlay overlay{{{0, 0, 40, 30}, annotationDocument().snapshot().base}, selection};
    overlay.setAnnotationContext(&document, &interaction, {0, 0, 40, 30});

    QMouseEvent press{QEvent::MouseButtonPress, QPointF{4, 5}, QPointF{4, 5},
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QMouseEvent move{QEvent::MouseMove, QPointF{18, 16}, QPointF{18, 16},
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier};
    QMouseEvent release{QEvent::MouseButtonRelease, QPointF{18, 16}, QPointF{18, 16},
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &press);
    QApplication::sendEvent(&overlay, &move);
    QApplication::sendEvent(&overlay, &release);

    CHECK(selection.rect().isEmpty());
    REQUIRE(document.objects().size() == 1);
    CHECK(std::get<RectangleAnnotation>(document.objects().front().payload).rect == QRectF(4, 5, 14, 11));
}

TEST_CASE("annotation overlay presents all Task 4 controls on the toolbar host") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    AnnotationInteraction interaction(document);
    SnipOverlay overlay{frozenMonitor(), selection};
    overlay.setToolbarHost(true);
    overlay.setAnnotationContext(&document, &interaction, {-40, 10, 40, 30});

    CHECK(overlay.findChild<QToolButton*>("selectToolButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("rectangleToolButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("ellipseToolButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("arrowToolButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("brushToolButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("colorButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("undoButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("redoButton") != nullptr);
    CHECK(overlay.findChild<QToolButton*>("deleteButton") != nullptr);
}

TEST_CASE("annotation overlay cancels a draft before requesting whole-session cancellation") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Arrow);
    interaction.press({2, 2});
    interaction.move({12, 8});
    SnipOverlay overlay{{{0, 0, 40, 30}, annotationDocument().snapshot().base}, selection};
    overlay.setAnnotationContext(&document, &interaction, {0, 0, 40, 30});
    int cancellations{};
    QObject::connect(&overlay, &SnipOverlay::cancelRequested, [&cancellations] { ++cancellations; });

    QKeyEvent escape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &escape);
    CHECK_FALSE(interaction.hasDraft());
    CHECK(cancellations == 0);
    QApplication::sendEvent(&overlay, &escape);
    CHECK(cancellations == 1);
}

TEST_CASE("annotation overlay clips one shared preview across monitor boundaries") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 20});
    auto document = annotationDocument({40, 20});
    REQUIRE(document.addObject(RectangleAnnotation{{5, 5, 30, 10}, {Qt::red, 3}}).has_value());
    AnnotationInteraction interaction(document);
    QImage image({20, 20}, QImage::Format_RGB32);
    image.fill(Qt::white);
    SnipOverlay left{{{0, 0, 20, 20}, image}, selection};
    SnipOverlay right{{{20, 0, 20, 20}, image}, selection};
    left.setToolbarHost(false);
    right.setToolbarHost(false);
    left.setAnnotationContext(&document, &interaction, {0, 0, 40, 20});
    right.setAnnotationContext(&document, &interaction, {0, 0, 40, 20});
    QImage leftPreview({20, 20}, QImage::Format_RGB32);
    leftPreview.fill(Qt::white);
    QImage rightPreview({20, 20}, QImage::Format_RGB32);
    rightPreview.fill(Qt::white);
    QPainter leftPainter(&leftPreview);
    left.render(&leftPainter);
    QPainter rightPainter(&rightPreview);
    right.render(&rightPainter);

    CHECK(leftPreview.pixelColor(5, 5) != QColor(Qt::white));
    CHECK(rightPreview.pixelColor(15, 5) != QColor(Qt::white));
    CHECK(leftPreview.pixelColor(19, 18) == QColor(Qt::white));
    CHECK(rightPreview.pixelColor(0, 18) == QColor(Qt::white));
}

TEST_CASE("annotation overlay replaces a moved draft in every shared preview") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 20});
    auto document = annotationDocument({40, 20});
    REQUIRE(document.addObject(RectangleAnnotation{{4, 5, 12, 8}, {Qt::red, 1}}).has_value());
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Select);
    interaction.press({10, 9});
    interaction.move({20, 9});
    REQUIRE(interaction.draft().has_value());

    QImage image({20, 20}, QImage::Format_RGB32);
    image.fill(Qt::white);
    SnipOverlay left{{{0, 0, 20, 20}, image}, selection};
    SnipOverlay right{{{20, 0, 20, 20}, image}, selection};
    left.setToolbarHost(false);
    right.setToolbarHost(false);
    left.setAnnotationContext(&document, &interaction, {0, 0, 40, 20});
    right.setAnnotationContext(&document, &interaction, {0, 0, 40, 20});
    QImage leftPreview({20, 20}, QImage::Format_RGB32);
    leftPreview.fill(Qt::white);
    QImage rightPreview({20, 20}, QImage::Format_RGB32);
    rightPreview.fill(Qt::white);
    QPainter leftPainter(&leftPreview);
    left.render(&leftPainter);
    leftPainter.end();
    QPainter rightPainter(&rightPreview);
    right.render(&rightPainter);
    rightPainter.end();

    CHECK(leftPreview.pixelColor(4, 5) == QColor(Qt::white));
    CHECK(leftPreview.pixelColor(14, 5) != QColor(Qt::white));
    CHECK(rightPreview.pixelColor(6, 5) != QColor(Qt::white));
}

TEST_CASE("annotation toolbar copy and save requests stay available without export wiring") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    AnnotationInteraction interaction(document);
    SnipOverlay overlay{frozenMonitor(), selection};
    overlay.setToolbarHost(true);
    overlay.setAnnotationContext(&document, &interaction, {-40, 10, 40, 30});
    auto* copy = overlay.findChild<QToolButton*>("copyButton");
    auto* save = overlay.findChild<QToolButton*>("saveButton");
    REQUIRE(copy != nullptr);
    REQUIRE(save != nullptr);
    CHECK(copy->isEnabled());
    CHECK(save->isEnabled());
    int copies{};
    int saves{};
    QObject::connect(&overlay, &SnipOverlay::copyRequested, [&copies] { ++copies; });
    QObject::connect(&overlay, &SnipOverlay::saveRequested, [&saves] { ++saves; });

    copy->click();
    save->click();

    CHECK(copies == 1);
    CHECK(saves == 1);
}
} // namespace
