#include "snip/SnipOverlay.hpp"
#include "annotation/AnnotationInteraction.hpp"
#include "annotation/AnnotationRenderer.hpp"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRegion>
#include <QSpinBox>
#include <QTextCursor>
#include <QToolButton>

#include <catch2/catch_test_macros.hpp>

#include <functional>

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

void sendDoubleClickSequence(QWidget& widget, QPointF point,
                             std::function<void()> betweenDoubleClickAndRelease = {}) {
    const auto send = [&widget, point](QEvent::Type type) {
        QMouseEvent event{type, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(&widget, &event);
    };
    send(QEvent::MouseButtonPress);
    send(QEvent::MouseButtonRelease);
    send(QEvent::MouseButtonPress);
    send(QEvent::MouseButtonDblClick);
    if (betweenDoubleClickAndRelease)
        betweenDoubleClickAndRelease();
    send(QEvent::MouseButtonRelease);
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

TEST_CASE("annotation mosaic toolbar selects the tool and exposes its bounded block control") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    AnnotationInteraction interaction(document);
    SnipOverlay overlay{frozenMonitor(), selection};
    overlay.setToolbarHost(true);
    overlay.setAnnotationContext(&document, &interaction, {-40, 10, 40, 30});

    auto* mosaic = overlay.findChild<QToolButton*>("mosaicToolButton");
    auto* blockSize = overlay.findChild<QSpinBox*>("mosaicBlockSizeSpinBox");
    REQUIRE(mosaic != nullptr);
    REQUIRE(blockSize != nullptr);
    CHECK(blockSize->minimum() == 1);
    CHECK(blockSize->maximum() == 128);
    CHECK(blockSize->value() == 12);

    QObject::connect(&overlay, &SnipOverlay::annotationToolRequested, [&interaction](AnnotationTool tool) {
        interaction.setTool(tool);
    });
    mosaic->click();
    blockSize->setValue(7);
    CHECK(interaction.tool() == AnnotationTool::Mosaic);
    CHECK(interaction.mosaicBlockSize() == 7);
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

TEST_CASE("annotation mosaic overlay previews join exactly at a monitor boundary") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 20});
    QImage base({40, 20}, QImage::Format_RGB32);
    for (int y = 0; y < base.height(); ++y) {
        for (int x = 0; x < base.width(); ++x)
            base.setPixelColor(x, y, {2 * x + y, 3 * x + 2 * y, 4 * x + 3 * y});
    }
    AnnotationDocument document(base);
    REQUIRE(document.addObject(MosaicAnnotation{{5, 1, 31, 18}, 7}).has_value());
    AnnotationInteraction interaction(document);
    SnipOverlay left{{{0, 0, 20, 20}, base.copy(0, 0, 20, 20)}, selection};
    SnipOverlay right{{{20, 0, 20, 20}, base.copy(20, 0, 20, 20)}, selection};
    left.setToolbarHost(false);
    right.setToolbarHost(false);
    left.setAnnotationContext(&document, &interaction, {0, 0, 40, 20});
    right.setAnnotationContext(&document, &interaction, {0, 0, 40, 20});

    QImage assembled({40, 20}, QImage::Format_RGB32);
    QPainter painter(&assembled);
    left.render(&painter, {0, 0}, QRegion{0, 0, 20, 20});
    right.render(&painter, {20, 0}, QRegion{0, 0, 20, 20});
    painter.end();
    CHECK(assembled == composeAnnotations(document.snapshot()));
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

TEST_CASE("annotation text editor exists only on the active toolbar host and commits once") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Text);
    QImage image({40, 30}, QImage::Format_RGB32);
    image.fill(Qt::white);
    SnipOverlay passive{{{0, 0, 40, 30}, image}, selection};
    SnipOverlay host{{{0, 0, 40, 30}, image}, selection};
    passive.setToolbarHost(false);
    host.setToolbarHost(true);
    passive.setAnnotationContext(&document, &interaction, {0, 0, 40, 30});
    host.setAnnotationContext(&document, &interaction, {0, 0, 40, 30});
    QObject::connect(&passive, &SnipOverlay::annotationTextCreateRequested, &host,
                     [&host](QPointF anchor) { host.createTextEditor(anchor); });

    QMouseEvent press{QEvent::MouseButtonPress, QPointF{5, 6}, QPointF{5, 6},
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(&passive, &press);
    CHECK(passive.findChild<QPlainTextEdit*>("annotationTextEditor") == nullptr);
    auto* editor = host.findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(editor != nullptr);
    editor->setPlainText(QStringLiteral("first"));
    QKeyEvent commit{QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier};
    QApplication::sendEvent(editor, &commit);

    CHECK(host.findChild<QPlainTextEdit*>("annotationTextEditor") == nullptr);
    REQUIRE(document.objects().size() == 1);
    CHECK(std::get<TextAnnotation>(document.objects().front().payload).anchor == QPointF(5, 6));
    CHECK(std::get<TextAnnotation>(document.objects().front().payload).text == QStringLiteral("first"));
    REQUIRE(document.undo());
    CHECK(document.objects().empty());
}

TEST_CASE("annotation text editor keeps editing shortcuts local and escapes without copying") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Text);
    SnipOverlay overlay{frozenMonitor(), selection};
    overlay.setToolbarHost(true);
    overlay.setAnnotationContext(&document, &interaction, {-40, 10, 40, 30});
    int copies{};
    int saves{};
    QObject::connect(&overlay, &SnipOverlay::copyRequested, [&copies] { ++copies; });
    QObject::connect(&overlay, &SnipOverlay::saveRequested, [&saves] { ++saves; });

    QMouseEvent press{QEvent::MouseButtonPress, QPointF{5, 6}, QPointF{5, 6},
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &press);
    auto* editor = overlay.findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(editor != nullptr);
    editor->setPlainText(QStringLiteral("a"));
    editor->moveCursor(QTextCursor::End);
    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n")};
    QApplication::sendEvent(editor, &enter);
    CHECK(editor->toPlainText() == QStringLiteral("a\n"));
    QKeyEvent undo{QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier};
    QApplication::sendEvent(editor, &undo);
    CHECK(editor->toPlainText() == QStringLiteral("a"));
    QKeyEvent redoWithControlY{QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier};
    QApplication::sendEvent(editor, &redoWithControlY);
    CHECK(editor->toPlainText() == QStringLiteral("a\n"));
    QApplication::sendEvent(editor, &undo);
    CHECK(editor->toPlainText() == QStringLiteral("a"));
    QKeyEvent redo{QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier};
    QApplication::sendEvent(editor, &redo);
    CHECK(editor->toPlainText() == QStringLiteral("a\n"));
    editor->selectAll();
    QKeyEvent copy{QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier};
    QApplication::sendEvent(editor, &copy);
    CHECK(QApplication::clipboard()->text() == QStringLiteral("a\n"));
    QKeyEvent erase{QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier};
    QApplication::sendEvent(editor, &erase);
    CHECK(editor->toPlainText().isEmpty());
    QKeyEvent save{QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier};
    QApplication::sendEvent(editor, &save);
    CHECK(copies == 0);
    CHECK(saves == 0);

    QKeyEvent escape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    QApplication::sendEvent(editor, &escape);
    CHECK(overlay.findChild<QPlainTextEdit*>("annotationTextEditor") == nullptr);
    CHECK(document.objects().empty());
}

TEST_CASE("double-clicking text reopens one editor without requesting screenshot copy") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    const auto id = document.addObject(TextAnnotation{{6, 5}, QStringLiteral("old"), {Qt::red, 24}});
    REQUIRE(id.has_value());
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Select);
    SnipOverlay overlay{{{0, 0, 40, 30}, annotationDocument().snapshot().base}, selection};
    overlay.setToolbarHost(true);
    overlay.setAnnotationContext(&document, &interaction, {0, 0, 40, 30});
    int copies{};
    QObject::connect(&overlay, &SnipOverlay::copyRequested, [&copies] { ++copies; });

    QMouseEvent doubleClick{QEvent::MouseButtonDblClick, QPointF{7, 6}, QPointF{7, 6},
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &doubleClick);
    auto* editor = overlay.findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(editor != nullptr);
    CHECK(editor->toPlainText() == QStringLiteral("old"));
    CHECK(copies == 0);
    editor->setPlainText(QStringLiteral("new"));
    QKeyEvent commit{QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier};
    QApplication::sendEvent(editor, &commit);
    CHECK(document.objects().size() == 1);
    CHECK(std::get<TextAnnotation>(document.objects().front().payload).text == QStringLiteral("new"));
    REQUIRE(document.undo());
    CHECK(std::get<TextAnnotation>(document.objects().front().payload).text == QStringLiteral("old"));
}

TEST_CASE("a real double-click edits text without retaining a creation or selection draft") {
    const auto verify = [](AnnotationTool tool) {
        SelectionModel selection;
        selection.setBounds({0, 0, 40, 30});
        auto document = annotationDocument();
        const auto id = document.addObject(TextAnnotation{{6, 5}, QStringLiteral("old"), {Qt::red, 12}});
        REQUIRE(id.has_value());
        AnnotationInteraction interaction(document);
        interaction.setTool(tool);
        SnipOverlay overlay{{{0, 0, 40, 30}, annotationDocument().snapshot().base}, selection};
        overlay.setToolbarHost(true);
        overlay.setAnnotationContext(&document, &interaction, {0, 0, 40, 30});

        sendDoubleClickSequence(overlay, {7, 6}, [&interaction] {
            CHECK_FALSE(interaction.hasDraft());
        });

        auto* editor = overlay.findChild<QPlainTextEdit*>("annotationTextEditor");
        REQUIRE(editor != nullptr);
        CHECK(editor->toPlainText() == QStringLiteral("old"));
        editor->setPlainText(QStringLiteral("new"));
        QKeyEvent commit{QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier};
        QApplication::sendEvent(editor, &commit);
        CHECK_FALSE(interaction.hasDraft());
        REQUIRE(document.objects().size() == 1);
        CHECK(std::get<TextAnnotation>(document.objects().front().payload).text == QStringLiteral("new"));
    };

    ApplicationFixture fixture;
    verify(AnnotationTool::Text);
    verify(AnnotationTool::Select);
}

TEST_CASE("a real select double-click clears its draft before opening text editing") {
    ApplicationFixture fixture;
    SelectionModel selection;
    selection.setBounds({0, 0, 40, 30});
    auto document = annotationDocument();
    REQUIRE(document.addObject(TextAnnotation{{6, 5}, QStringLiteral("old"), {Qt::red, 12}}).has_value());
    AnnotationInteraction interaction(document);
    interaction.setTool(AnnotationTool::Select);
    SnipOverlay overlay{{{0, 0, 40, 30}, annotationDocument().snapshot().base}, selection};
    overlay.setToolbarHost(true);
    overlay.setAnnotationContext(&document, &interaction, {0, 0, 40, 30});

    sendDoubleClickSequence(overlay, {7, 6}, [&interaction] {
        CHECK_FALSE(interaction.hasDraft());
    });

    auto* editor = overlay.findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(editor != nullptr);
    editor->setPlainText(QStringLiteral("new"));
    QKeyEvent commit{QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier};
    QApplication::sendEvent(editor, &commit);
    CHECK_FALSE(interaction.hasDraft());
    CHECK(std::get<TextAnnotation>(document.objects().front().payload).text == QStringLiteral("new"));
}
} // namespace
