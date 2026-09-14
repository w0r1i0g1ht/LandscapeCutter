#include "pin/PinWindow.hpp"

#include "../annotation/AnnotationTestApplication.hpp"
#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationTypes.hpp"

#include <QApplication>
#include <QAction>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>
#include <QWheelEvent>
#include <QPlainTextEdit>
#include <QPointer>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace {
using Catch::Approx;
using lc::annotation::AnnotationDocument;
using lc::pin::PinWindow;
using lc::pin::PinWindowMode;

std::unique_ptr<AnnotationDocument> makeDocument(QSize size = {100, 50}) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);
    return std::make_unique<AnnotationDocument>(image);
}

void sendWheel(PinWindow& window, QPoint local, int delta, Qt::KeyboardModifiers modifiers) {
    const QPoint global = window.mapToGlobal(local);
    QWheelEvent event(QPointF(local), QPointF(global), {}, {0, delta}, Qt::NoButton, modifiers,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(&window, &event);
}

void sendMouse(PinWindow& window, QEvent::Type type, QPointF local, QPointF global,
               Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent event(type, local, global, button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&window, &event);
}

TEST_CASE("pin window starts frameless topmost and owns its document") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(1);
    auto document = makeDocument({80, 40});
    auto* original = document.get();

    const auto error = window->attachDocument(document, {20, 30});

    CHECK(error.isEmpty());
    CHECK(document == nullptr);
    CHECK(window->document() == original);
    CHECK(window->geometry() == QRect(20, 30, 80, 40));
    CHECK(window->windowFlags().testFlag(Qt::FramelessWindowHint));
    CHECK(window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    CHECK(window->windowFlags().testFlag(Qt::Tool));
    CHECK(window->mode() == PinWindowMode::Viewing);
}

TEST_CASE("pin window wheel routes zoom and opacity separately") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(2);
    auto document = makeDocument();
    REQUIRE(window->attachDocument(document, {0, 0}).isEmpty());

    sendWheel(*window, {50, 25}, 120, Qt::NoModifier);
    CHECK(window->size() == QSize(110, 55));
    CHECK(window->pos() == QPoint(-5, -3));

    sendWheel(*window, {50, 25}, -120, Qt::ControlModifier);
    CHECK(window->windowOpacity() == Approx(0.95).margin(0.01));
    CHECK(window->size() == QSize(110, 55));
}

TEST_CASE("pin window dragging does not move while editing") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(3);
    auto document = makeDocument();
    REQUIRE(window->attachDocument(document, {40, 50}).isEmpty());
    QCoreApplication::processEvents();
    window->enterEditing();
    const QPointF local{10, 10};
    QMouseEvent press{QEvent::MouseButtonPress, local, QPointF{50, 60}, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier};
    QMouseEvent move{QEvent::MouseMove, QPointF{30, 25}, QPointF{70, 75}, Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier};
    QMouseEvent release{QEvent::MouseButtonRelease, QPointF{30, 25}, QPointF{70, 75},
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier};

    QApplication::sendEvent(window.get(), &press);
    QApplication::sendEvent(window.get(), &move);
    QApplication::sendEvent(window.get(), &release);

    CHECK(window->pos() == QPoint(40, 50));
}

TEST_CASE("pin window double click enters editing and shows the shared toolbar") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(4);
    auto document = makeDocument();
    REQUIRE(window->attachDocument(document, {0, 0}).isEmpty());
    QMouseEvent doubleClick{QEvent::MouseButtonDblClick, QPointF{20, 20}, QPointF{20, 20},
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};

    QApplication::sendEvent(window.get(), &doubleClick);

    CHECK(window->mode() == PinWindowMode::Editing);
    auto* done = window->findChild<QToolButton*>("doneButton");
    REQUIRE(done != nullptr);
    CHECK_FALSE(done->isHidden());
}

TEST_CASE("pin window editing keeps history and returns to viewing on done") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(5);
    auto document = makeDocument({100, 60});
    REQUIRE(document->addObject(lc::annotation::RectangleAnnotation{{5, 5, 20, 10}, {Qt::red, 3}})
                .has_value());
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    window->enterEditing();
    REQUIRE(window->document()->undo());
    CHECK(window->document()->objects().empty());
    window->finishEditing();
    CHECK(window->mode() == PinWindowMode::Viewing);
}

TEST_CASE("pin window commits nonempty text and keeps it editable") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(6);
    auto document = makeDocument({120, 80});
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    window->enterEditing();
    auto* text = window->findChild<QToolButton*>("textToolButton");
    REQUIRE(text != nullptr);
    text->click();
    QMouseEvent press{QEvent::MouseButtonPress, QPointF{10, 10}, QPointF{10, 10},
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(window.get(), &press);
    auto* editor = window->findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(editor != nullptr);
    editor->setPlainText("editable");
    window->finishEditing();
    REQUIRE(window->document()->objects().size() == 1);
    CHECK(window->mode() == PinWindowMode::Viewing);
    window->enterEditing();
    CHECK(window->document()->objects().size() == 1);
}

TEST_CASE("pin window routes rectangle selection history and delete while editing") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(7);
    auto document = makeDocument({120, 80});
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    window->enterEditing();
    auto* rectangle = window->findChild<QToolButton*>("rectangleToolButton");
    REQUIRE(rectangle != nullptr);
    rectangle->click();
    sendMouse(*window, QEvent::MouseButtonPress, {10, 10}, {10, 10}, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(*window, QEvent::MouseMove, {30, 25}, {30, 25}, Qt::NoButton, Qt::LeftButton);
    sendMouse(*window, QEvent::MouseButtonRelease, {30, 25}, {30, 25}, Qt::LeftButton,
              Qt::NoButton);
    REQUIRE(window->document()->objects().size() == 1);
    const auto original = std::get<lc::annotation::RectangleAnnotation>(
                              window->document()->objects().front().payload)
                              .rect;

    auto* select = window->findChild<QToolButton*>("selectToolButton");
    REQUIRE(select != nullptr);
    select->click();
    sendMouse(*window, QEvent::MouseButtonPress, {20, 18}, {20, 18}, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(*window, QEvent::MouseMove, {35, 28}, {35, 28}, Qt::NoButton, Qt::LeftButton);
    sendMouse(*window, QEvent::MouseButtonRelease, {35, 28}, {35, 28}, Qt::LeftButton,
              Qt::NoButton);
    const auto moved = std::get<lc::annotation::RectangleAnnotation>(
                           window->document()->objects().front().payload)
                           .rect;
    CHECK(moved != original);

    QKeyEvent deleteKey{QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier};
    QApplication::sendEvent(window.get(), &deleteKey);
    CHECK(window->document()->objects().empty());
    QKeyEvent undo{QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier};
    QApplication::sendEvent(window.get(), &undo);
    REQUIRE(window->document()->objects().size() == 1);
    QKeyEvent redo{QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier};
    QApplication::sendEvent(window.get(), &redo);
    CHECK(window->document()->objects().empty());
}

TEST_CASE("pin window text double click edits text and blank double click finishes") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(8);
    auto document = makeDocument({120, 80});
    REQUIRE(document->addObject(lc::annotation::TextAnnotation{{10, 10}, "text", {Qt::red, 24}})
                .has_value());
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    window->enterEditing();
    sendMouse(*window, QEvent::MouseButtonDblClick, {12, 12}, {12, 12}, Qt::LeftButton,
              Qt::LeftButton);
    REQUIRE(window->findChild<QPlainTextEdit*>("annotationTextEditor") != nullptr);
    QKeyEvent escape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    QApplication::sendEvent(window.get(), &escape);
    sendMouse(*window, QEvent::MouseButtonDblClick, {100, 70}, {100, 70}, Qt::LeftButton,
              Qt::LeftButton);
    CHECK(window->mode() == PinWindowMode::Viewing);
}

TEST_CASE("pin window non-text double click stays editing without creating text") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(9);
    auto document = makeDocument();
    REQUIRE(document->addObject(lc::annotation::RectangleAnnotation{{5, 5, 30, 20}, {Qt::red, 3}})
                .has_value());
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    window->enterEditing();
    sendMouse(*window, QEvent::MouseButtonDblClick, {10, 10}, {10, 10}, Qt::LeftButton,
              Qt::LeftButton);
    CHECK(window->mode() == PinWindowMode::Editing);
    CHECK(window->findChild<QPlainTextEdit*>("annotationTextEditor") == nullptr);
}

TEST_CASE("pin window empty text is discarded and actions emit host signals") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(10);
    auto document = makeDocument();
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    window->enterEditing();
    auto* text = window->findChild<QToolButton*>("textToolButton");
    REQUIRE(text != nullptr);
    text->click();
    sendMouse(*window, QEvent::MouseButtonPress, {10, 10}, {10, 10}, Qt::LeftButton,
              Qt::LeftButton);
    REQUIRE(window->findChild<QPlainTextEdit*>("annotationTextEditor") != nullptr);
    window->finishEditing();
    CHECK(window->document()->objects().empty());

    int copyCount{};
    int saveCount{};
    auto* copy = window->findChild<QAction*>("pinCopyAction");
    auto* save = window->findChild<QAction*>("pinSaveAction");
    auto* edit = window->findChild<QAction*>("editPinAction");
    auto* resetSize = window->findChild<QAction*>("resetPinSizeAction");
    auto* resetOpacity = window->findChild<QAction*>("resetPinOpacityAction");
    auto* close = window->findChild<QAction*>("closePinAction");
    REQUIRE(copy != nullptr);
    REQUIRE(save != nullptr);
    REQUIRE(edit != nullptr);
    REQUIRE(resetSize != nullptr);
    REQUIRE(resetOpacity != nullptr);
    REQUIRE(close != nullptr);
    CHECK(copy->text() == QStringLiteral("复制"));
    CHECK(save->text() == QStringLiteral("另存为…"));
    CHECK(edit->text() == QStringLiteral("编辑"));
    CHECK(resetSize->text() == QStringLiteral("恢复原始大小"));
    CHECK(resetOpacity->text() == QStringLiteral("恢复不透明"));
    CHECK(close->text() == QStringLiteral("关闭"));

    QContextMenuEvent viewingContext{QContextMenuEvent::Mouse, {10, 10}, {10, 10}};
    QApplication::sendEvent(window.get(), &viewingContext);
    auto* menu = window->findChild<QMenu*>("pinContextMenu");
    REQUIRE(menu != nullptr);
    CHECK(menu->isVisible());
    menu->hide();

    window->enterEditing();
    text->click();
    sendMouse(*window, QEvent::MouseButtonPress, {20, 20}, {20, 20}, Qt::LeftButton,
              Qt::LeftButton);
    auto* copyEditor = window->findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(copyEditor != nullptr);
    copyEditor->setPlainText("copy text");
    bool copySawCommitted{};
    QObject::connect(window.get(), &PinWindow::copyRequested,
                     [&copyCount, &copySawCommitted, &window](const auto) {
                         ++copyCount;
                         copySawCommitted = window->document()->objects().size() == 1 &&
                                            window->findChild<QPlainTextEdit*>("annotationTextEditor") == nullptr;
                     });
    copy->trigger();

    text->click();
    sendMouse(*window, QEvent::MouseButtonPress, {100, 60}, {100, 60}, Qt::LeftButton,
              Qt::LeftButton);
    auto* saveEditor = window->findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(saveEditor != nullptr);
    saveEditor->setPlainText("save text");
    bool saveSawCommitted{};
    QObject::connect(window.get(), &PinWindow::saveRequested,
                     [&saveCount, &saveSawCommitted, &window](const auto) {
                         ++saveCount;
                         saveSawCommitted = window->document()->objects().size() == 2 &&
                                            window->findChild<QPlainTextEdit*>("annotationTextEditor") == nullptr;
                     });
    save->trigger();
    CHECK(copyCount == 1);
    CHECK(saveCount == 1);
    CHECK(copySawCommitted);
    CHECK(saveSawCommitted);

    QContextMenuEvent context{QContextMenuEvent::Mouse, {10, 10}, {10, 10}};
    context.setAccepted(true);
    QApplication::sendEvent(window.get(), &context);
    CHECK_FALSE(context.isAccepted());
}

TEST_CASE("pin window attach rejection preserves the caller document") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(11);
    auto first = makeDocument();
    REQUIRE(window->attachDocument(first, {}).isEmpty());
    auto rejected = makeDocument();
    auto* original = rejected.get();
    CHECK_FALSE(window->attachDocument(rejected, {}).isEmpty());
    CHECK(rejected.get() == original);
}

TEST_CASE("pin window escape cancels a draft before it leaves editing") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto window = std::make_unique<PinWindow>(7);
    auto document = makeDocument();
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    window->enterEditing();
    auto* rectangle = window->findChild<QToolButton*>("rectangleToolButton");
    REQUIRE(rectangle != nullptr);
    rectangle->click();
    QMouseEvent press{QEvent::MouseButtonPress, QPointF{10, 10}, QPointF{10, 10},
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(window.get(), &press);
    QKeyEvent firstEscape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    QApplication::sendEvent(window.get(), &firstEscape);
    CHECK(window->mode() == PinWindowMode::Editing);
    QKeyEvent secondEscape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    QApplication::sendEvent(window.get(), &secondEscape);
    CHECK(window->mode() == PinWindowMode::Viewing);
}

TEST_CASE("pin window emits its closed id exactly once") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    auto* window = new PinWindow(8);
    auto document = makeDocument();
    REQUIRE(window->attachDocument(document, {}).isEmpty());
    int closedCount{};
    lc::pin::PinId closedId{};
    QObject::connect(window, &PinWindow::closed, [&closedCount, &closedId](const auto id) {
        ++closedCount;
        closedId = id;
    });
    QPointer<PinWindow> guard(window);
    window->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(closedCount == 1);
    CHECK(closedId == 8);
    CHECK(guard == nullptr);
}
} // namespace
