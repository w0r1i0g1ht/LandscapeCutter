#include "snip/SnipOverlay.hpp"
#include "snip/SnipSession.hpp"
#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <functional>
namespace {
struct SessionService : lc::capture::IMonitorCaptureService {
    void captureOnce(lc::capture::MonitorCaptureRequest, lc::capture::CaptureCompletion) override {}
    void cancel() noexcept override {}
};
struct SessionRecovery : lc::graphics::d3d11::ID3d11DeviceRecovery {
    bool rebuild() override {
        return true;
    }
    std::uint64_t generation() const noexcept override {
        return 1;
    }
};

struct ControlledPreparation {
    lc::snip::PrepareAnnotation function() {
        return [this](std::vector<lc::snip::FrozenMonitor>, QRect selection,
                      std::shared_ptr<std::atomic_bool> cancelled,
                      std::function<void(QImage)> completion) {
            lockedSelection = selection;
            cancellation = std::move(cancelled);
            callbacks.push_back(std::move(completion));
        };
    }

    void complete(QImage image, std::size_t request = 0) {
        callbacks.at(request)(std::move(image));
    }

    QRect lockedSelection;
    std::shared_ptr<std::atomic_bool> cancellation;
    std::vector<std::function<void(QImage)>> callbacks;
};

struct ControlledSavePathChooser {
    lc::snip::ChooseSavePath function() {
        return [this](std::function<void(QString)> accepted, std::function<void()> rejected) {
            accept = std::move(accepted);
            reject = std::move(rejected);
        };
    }

    std::function<void(QString)> accept;
    std::function<void()> reject;
};

QImage annotationTestImage() {
    QImage image(20, 15, QImage::Format_RGB32);
    image.fill(Qt::red);
    return image;
}

void selectRect(lc::snip::SnipSession& session, QPoint start = {2, 3}, QPoint end = {12, 10}) {
    session.selection().press(start, 0);
    session.selection().move(end);
    session.selection().release();
}

std::vector<lc::snip::SnipOverlay*> activeOverlays() {
    std::vector<lc::snip::SnipOverlay*> overlays;
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (auto* overlay = qobject_cast<lc::snip::SnipOverlay*>(widget); overlay != nullptr) {
            overlays.push_back(overlay);
        }
    }
    return overlays;
}

int visibleToolbarCount(const std::vector<lc::snip::SnipOverlay*>& overlays) {
    return static_cast<int>(
        std::count_if(overlays.begin(), overlays.end(), [](const lc::snip::SnipOverlay* overlay) {
            const auto* toolbar = overlay->findChild<QWidget*>("snipToolbar");
            return toolbar != nullptr && !toolbar->isHidden();
        }));
}

void sendDoubleClickSequence(QWidget& widget, QPointF point) {
    const auto send = [&widget, point](QEvent::Type type) {
        QMouseEvent event{type, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(&widget, &event);
    };
    send(QEvent::MouseButtonPress);
    send(QEvent::MouseButtonRelease);
    send(QEvent::MouseButtonPress);
    send(QEvent::MouseButtonDblClick);
    send(QEvent::MouseButtonRelease);
}
} // namespace
TEST_CASE("snip session opens frozen overlays together and cancellation closes them") {
    int argc = 1;
    char name[] = "session-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    lc::snip::SnipSession session(batch);
    CHECK(session.overlayCount() == 0);
    QImage red(100, 100, QImage::Format_RGB32);
    red.fill(Qt::red);
    session.begin({}); // empty catalog fails, so unsolicited completion must be ignored
    batch.ready({{{0, 0, 100, 100}, red}});
    CHECK(session.overlayCount() == 0);
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 100, 100};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    CHECK(session.state() == lc::snip::SnipSessionState::PreparingCapture);
    batch.ready({{{0, 0, 100, 100}, red}, {{100, 0, 100, 100}, red}});
    CHECK(session.overlayCount() == 2);
    session.cancel();
    CHECK(session.overlayCount() == 0);
    CHECK_FALSE(session.active());
}
TEST_CASE("snip session copies the selected frozen physical pixels then closes") {
    int argc = 1;
    char name[] = "session-copy-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    lc::snip::SnipSession session(batch);
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 100, 100};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    QImage red(100, 100, QImage::Format_RGB32);
    red.fill(Qt::red);
    batch.ready({{{0, 0, 100, 100}, red}});
    session.selection().press({10, 10}, 0);
    session.selection().move({30, 25});
    session.selection().release();
    session.copy();
    CHECK(session.state() == lc::snip::SnipSessionState::ExportingFromSelection);
    QElapsedTimer timer;
    timer.start();
    while (session.active() && timer.elapsed() < 3000) {
        app.processEvents();
        QThread::msleep(1);
    }
    CHECK_FALSE(session.active());
    REQUIRE(app.clipboard()->image().size() == QSize(20, 15));
    CHECK(app.clipboard()->image().pixelColor(0, 0) == QColor(Qt::red));
}

TEST_CASE("annotation preparation locks the selected rectangle before completion") {
    int argc = 1;
    char name[] = "annotation-lock-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);

    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    CHECK(session.state() == lc::snip::SnipSessionState::PreparingAnnotation);
    CHECK(preparation.lockedSelection == QRect(2, 3, 10, 7));
    CHECK(session.lockedSelection() == QRect(2, 3, 10, 7));
    selectRect(session, {4, 4}, {16, 12});
    CHECK(session.lockedSelection() == QRect(2, 3, 10, 7));
    preparation.complete(QImage(10, 7, QImage::Format_RGB32));
    app.processEvents();

    REQUIRE(session.document() != nullptr);
    CHECK(session.document()->snapshot().base.size() == QSize(10, 7));
    CHECK(session.lockedSelection() == QRect(2, 3, 10, 7));
    CHECK(session.state() == lc::snip::SnipSessionState::Annotating);
    session.cancel();
}

TEST_CASE("empty annotation preparation returns to selecting with selection preserved") {
    int argc = 1;
    char name[] = "annotation-empty-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    const QRect selection = session.selection().rect();
    int errors{};
    QObject::connect(&session, &lc::snip::SnipSession::errorOccurred,
                     [&errors](const QString&) { ++errors; });

    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    preparation.complete({});
    app.processEvents();

    CHECK(session.state() == lc::snip::SnipSessionState::Selecting);
    CHECK(session.selection().rect() == selection);
    CHECK(session.document() == nullptr);
    CHECK(errors == 1);
    session.cancel();
}

TEST_CASE("cancelled annotation preparation cannot resurrect a snip session") {
    int argc = 1;
    char name[] = "annotation-cancel-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);

    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    session.cancel();
    preparation.complete(annotationTestImage());
    app.processEvents();

    CHECK(session.state() == lc::snip::SnipSessionState::Idle);
    CHECK(session.document() == nullptr);
}

TEST_CASE("stale annotation preparation cannot complete a later session") {
    int argc = 1;
    char name[] = "annotation-stale-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;

    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    session.cancel();

    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    session.beginAnnotation(lc::annotation::AnnotationTool::Ellipse);
    preparation.complete(annotationTestImage(), 0);
    app.processEvents();

    CHECK(session.state() == lc::snip::SnipSessionState::PreparingAnnotation);
    CHECK(session.document() == nullptr);
    preparation.complete(annotationTestImage(), 1);
    app.processEvents();
    CHECK(session.state() == lc::snip::SnipSessionState::Annotating);
    session.cancel();
}

TEST_CASE("failed annotation preparation cannot complete its same-session retry") {
    int argc = 1;
    char name[] = "annotation-retry-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);

    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    preparation.complete({}, 0);
    app.processEvents();
    REQUIRE(session.state() == lc::snip::SnipSessionState::Selecting);
    session.beginAnnotation(lc::annotation::AnnotationTool::Ellipse);
    preparation.complete(annotationTestImage(), 0);
    app.processEvents();

    CHECK(session.state() == lc::snip::SnipSessionState::PreparingAnnotation);
    CHECK(session.document() == nullptr);
    preparation.complete(annotationTestImage(), 1);
    app.processEvents();
    CHECK(session.state() == lc::snip::SnipSessionState::Annotating);
    session.cancel();
}

TEST_CASE("annotation preparation exceptions return to selecting and report an error") {
    int argc = 1;
    char name[] = "annotation-exception-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    lc::snip::SnipSession session(batch, [](std::vector<lc::snip::FrozenMonitor>, QRect,
                                            std::shared_ptr<std::atomic_bool>,
                                            std::function<void(QImage)>) { throw 1; });
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    const QRect selection = session.selection().rect();
    int errors{};
    QObject::connect(&session, &lc::snip::SnipSession::errorOccurred,
                     [&errors](const QString&) { ++errors; });

    CHECK_NOTHROW(session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle));
    app.processEvents();
    CHECK(session.state() == lc::snip::SnipSessionState::Selecting);
    CHECK(session.selection().rect() == selection);
    CHECK(errors == 1);
    session.cancel();
}

TEST_CASE("direct save enters the save-path state without annotation") {
    int argc = 1;
    char name[] = "selection-save-state-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledSavePathChooser chooser;
    lc::snip::SnipSession session(batch, lc::snip::PrepareAnnotation{}, chooser.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    session.save();
    CHECK(session.state() == lc::snip::SnipSessionState::ChoosingSavePath);
    chooser.reject();
    CHECK(session.state() == lc::snip::SnipSessionState::Selecting);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    session.save();
    chooser.accept(directory.filePath("selection.png"));
    CHECK(session.state() == lc::snip::SnipSessionState::ExportingFromSelection);
    session.cancel();
}

TEST_CASE("closing any snip overlay cancels the whole session") {
    int argc = 1;
    char name[] = "session-close-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    lc::snip::SnipSession session(batch);
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 100, 100};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    QImage red(100, 100, QImage::Format_RGB32);
    red.fill(Qt::red);
    batch.ready({{{0, 0, 100, 100}, red}});
    REQUIRE(session.overlayCount() == 1);
    auto overlays = QApplication::topLevelWidgets();
    const auto found = std::find_if(overlays.begin(), overlays.end(), [](QWidget* widget) {
        return qobject_cast<lc::snip::SnipOverlay*>(widget) != nullptr;
    });
    REQUIRE(found != overlays.end());

    (*found)->close();
    app.processEvents();

    CHECK_FALSE(session.active());
    CHECK(session.overlayCount() == 0);
}

TEST_CASE("a composition error leaves the snip session retryable") {
    int argc = 1;
    char name[] = "session-error-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    lc::snip::SnipSession session(batch);
    lc::platform::windows::MonitorDescriptor left{}, right{};
    left.desktopRect = {-2, 0, 0, 2};
    left.catalogGeneration = 1;
    right.desktopRect = {1, 0, 3, 2};
    right.catalogGeneration = 1;
    session.begin({left, right});
    QImage red(2, 2, QImage::Format_RGB32);
    red.fill(Qt::red);
    batch.ready({{{-2, 0, 2, 2}, red}, {{1, 0, 2, 2}, red}});
    session.selection().press({0, 0}, 0);
    session.selection().move({1, 1});
    session.selection().release();
    int errors{};
    QObject::connect(&session, &lc::snip::SnipSession::errorOccurred,
                     [&errors](const QString&) { ++errors; });

    session.copy();
    QElapsedTimer timer;
    timer.start();
    while (errors == 0 && timer.elapsed() < 3000) {
        app.processEvents();
        QThread::msleep(1);
    }

    CHECK(errors == 1);
    CHECK(session.active());
    CHECK(session.overlayCount() == 2);
    session.cancel();
}

TEST_CASE("a cross-monitor selection has exactly one toolbar") {
    int argc = 1;
    char name[] = "session-toolbar-cross-monitor-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    lc::snip::SnipSession session(batch);
    lc::platform::windows::MonitorDescriptor left{}, right{};
    left.desktopRect = {0, 0, 100, 100};
    left.catalogGeneration = 1;
    right.desktopRect = {120, 0, 220, 100};
    right.catalogGeneration = 1;
    session.begin({left, right});
    QImage image(100, 100, QImage::Format_RGB32);
    image.fill(Qt::red);
    batch.ready({{{0, 0, 100, 100}, image}, {{120, 0, 100, 100}, image}});
    const auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 2);

    session.selection().press({90, 10}, 0);
    session.selection().move({130, 30});
    session.selection().release();
    overlays.front()->selectionChanged();
    app.processEvents();

    CHECK(visibleToolbarCount(overlays) == 1);
    session.cancel();
}

TEST_CASE("a selection in a monitor gap keeps one toolbar available") {
    int argc = 1;
    char name[] = "session-toolbar-gap-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    lc::snip::SnipSession session(batch);
    lc::platform::windows::MonitorDescriptor left{}, right{};
    left.desktopRect = {0, 0, 100, 100};
    left.catalogGeneration = 1;
    right.desktopRect = {120, 0, 220, 100};
    right.catalogGeneration = 1;
    session.begin({left, right});
    QImage image(100, 100, QImage::Format_RGB32);
    image.fill(Qt::red);
    batch.ready({{{0, 0, 100, 100}, image}, {{120, 0, 100, 100}, image}});
    const auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 2);

    session.selection().press({105, 10}, 0);
    session.selection().move({115, 30});
    session.selection().release();
    overlays.front()->selectionChanged();
    app.processEvents();

    CHECK(visibleToolbarCount(overlays) == 1);
    session.cancel();
}

TEST_CASE("annotation toolbar tools begin preparation from selecting") {
    int argc = 1;
    char name[] = "annotation-toolbar-start-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    const auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 1);
    overlays.front()->selectionChanged();
    app.processEvents();
    auto* rectangle = overlays.front()->findChild<QToolButton*>("rectangleToolButton");
    REQUIRE(rectangle != nullptr);
    CHECK(rectangle->isVisible());

    rectangle->click();

    CHECK(session.state() == lc::snip::SnipSessionState::PreparingAnnotation);
    CHECK(preparation.lockedSelection == QRect(2, 3, 10, 7));
    session.cancel();
}

TEST_CASE("annotation session shares one interaction across overlay input") {
    int argc = 1;
    char name[] = "annotation-shared-interaction-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    preparation.complete(QImage(10, 7, QImage::Format_RGB32));
    app.processEvents();
    REQUIRE(session.state() == lc::snip::SnipSessionState::Annotating);
    REQUIRE(session.interaction() != nullptr);
    const auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 1);

    QMouseEvent press{QEvent::MouseButtonPress, QPointF{3, 4}, QPointF{3, 4},
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QMouseEvent move{QEvent::MouseMove, QPointF{10, 8}, QPointF{10, 8},
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier};
    QMouseEvent release{QEvent::MouseButtonRelease, QPointF{10, 8}, QPointF{10, 8},
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier};
    QApplication::sendEvent(overlays.front(), &press);
    QApplication::sendEvent(overlays.front(), &move);
    QApplication::sendEvent(overlays.front(), &release);

    REQUIRE(session.document()->objects().size() == 1);
    CHECK(std::get<lc::annotation::RectangleAnnotation>(session.document()->objects().front().payload).rect ==
          QRectF(1, 1, 7, 4));
    auto* undo = overlays.front()->findChild<QToolButton*>("undoButton");
    auto* redo = overlays.front()->findChild<QToolButton*>("redoButton");
    auto* remove = overlays.front()->findChild<QToolButton*>("deleteButton");
    REQUIRE(undo != nullptr);
    REQUIRE(redo != nullptr);
    REQUIRE(remove != nullptr);
    CHECK(undo->isEnabled());
    CHECK_FALSE(redo->isEnabled());
    CHECK(remove->isEnabled());

    QKeyEvent deleteKey{QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier};
    QApplication::sendEvent(overlays.front(), &deleteKey);
    CHECK(session.document()->objects().empty());
    CHECK(undo->isEnabled());
    CHECK_FALSE(redo->isEnabled());
    CHECK_FALSE(remove->isEnabled());

    QKeyEvent undoKey{QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier};
    QApplication::sendEvent(overlays.front(), &undoKey);
    REQUIRE(session.document()->objects().size() == 1);
    CHECK(undo->isEnabled());
    CHECK(redo->isEnabled());
    CHECK(remove->isEnabled());

    QKeyEvent redoKey{QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier};
    QApplication::sendEvent(overlays.front(), &redoKey);
    CHECK(session.document()->objects().empty());
    CHECK(undo->isEnabled());
    CHECK_FALSE(redo->isEnabled());
    CHECK_FALSE(remove->isEnabled());
    session.cancel();
}

TEST_CASE("annotation cancellation clears deferred overlay contexts before destroying the document") {
    int argc = 1;
    char name[] = "annotation-cancel-overlay-context-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor monitor{};
    monitor.desktopRect = {0, 0, 20, 15};
    monitor.catalogGeneration = 1;
    session.begin({monitor});
    batch.ready({{{0, 0, 20, 15}, annotationTestImage()}});
    selectRect(session);
    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    preparation.complete(QImage(10, 7, QImage::Format_RGB32));
    app.processEvents();
    const auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 1);
    auto* deferredOverlay = overlays.front();

    session.cancel();
    CHECK_NOTHROW(deferredOverlay->refresh());
    QImage preview(20, 15, QImage::Format_RGB32);
    QPainter painter(&preview);
    CHECK_NOTHROW(deferredOverlay->render(&painter));
    painter.end();
    app.processEvents();
}

TEST_CASE("annotating across monitors keeps one toolbar host while a draft is live") {
    int argc = 1;
    char name[] = "annotation-live-draft-toolbar-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor left{}, right{};
    left.desktopRect = {0, 0, 20, 20};
    left.catalogGeneration = 1;
    right.desktopRect = {20, 0, 40, 20};
    right.catalogGeneration = 1;
    session.begin({left, right});
    QImage image(20, 20, QImage::Format_RGB32);
    image.fill(Qt::white);
    batch.ready({{{0, 0, 20, 20}, image}, {{20, 0, 20, 20}, image}});
    session.selection().press({5, 5}, 0);
    session.selection().move({35, 15});
    session.selection().release();
    auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 2);
    overlays.front()->selectionChanged();
    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    preparation.complete(QImage(30, 10, QImage::Format_RGB32));
    app.processEvents();
    REQUIRE(session.interaction() != nullptr);
    session.interaction()->press({4, 4});
    REQUIRE(session.interaction()->hasDraft());
    overlays.front()->annotationChanged();

    CHECK(visibleToolbarCount(overlays) == 1);
    session.cancel();
}

TEST_CASE("a text click on a non-host overlay creates the editor on the toolbar host") {
    int argc = 1;
    char name[] = "annotation-text-host-routing-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor left{}, right{};
    left.desktopRect = {0, 0, 20, 20};
    left.catalogGeneration = 1;
    right.desktopRect = {20, 0, 40, 20};
    right.catalogGeneration = 1;
    session.begin({left, right});
    QImage image(20, 20, QImage::Format_RGB32);
    image.fill(Qt::white);
    batch.ready({{{0, 0, 20, 20}, image}, {{20, 0, 20, 20}, image}});
    session.selection().press({5, 5}, 0);
    session.selection().move({35, 15});
    session.selection().release();
    session.beginAnnotation(lc::annotation::AnnotationTool::Text);
    preparation.complete(QImage(30, 10, QImage::Format_RGB32));
    app.processEvents();
    const auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 2);
    const auto host = std::find_if(overlays.begin(), overlays.end(),
                                   [](const auto* overlay) { return overlay->isToolbarHost(); });
    REQUIRE(host != overlays.end());
    const auto passive = std::find_if(overlays.begin(), overlays.end(),
                                      [host](const auto* overlay) { return overlay != *host; });
    REQUIRE(passive != overlays.end());

    QMouseEvent press{QEvent::MouseButtonPress, QPointF{3, 3}, QPointF{3, 3},
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(*passive, &press);

    CHECK((*passive)->findChild<QPlainTextEdit*>("annotationTextEditor") == nullptr);
    CHECK((*host)->findChild<QPlainTextEdit*>("annotationTextEditor") != nullptr);
    session.cancel();
}

TEST_CASE("a non-host text double-click replaces an empty host draft with the hit object") {
    int argc = 1;
    char name[] = "annotation-text-host-double-click-test";
    char* argv[] = {name, nullptr};
    QApplication app(argc, argv);
    SessionService service;
    SessionRecovery recovery;
    lc::snip::SnapshotBatch batch(service, recovery, {});
    ControlledPreparation preparation;
    lc::snip::SnipSession session(batch, preparation.function());
    lc::platform::windows::MonitorDescriptor left{}, right{};
    left.desktopRect = {0, 0, 20, 20};
    left.catalogGeneration = 1;
    right.desktopRect = {20, 0, 40, 20};
    right.catalogGeneration = 1;
    session.begin({left, right});
    QImage image(20, 20, QImage::Format_RGB32);
    image.fill(Qt::white);
    batch.ready({{{0, 0, 20, 20}, image}, {{20, 0, 20, 20}, image}});
    session.selection().press({5, 5}, 0);
    session.selection().move({35, 15});
    session.selection().release();
    session.beginAnnotation(lc::annotation::AnnotationTool::Text);
    preparation.complete(QImage(30, 10, QImage::Format_RGB32));
    app.processEvents();
    const auto overlays = activeOverlays();
    REQUIRE(overlays.size() == 2);
    const auto host = std::find_if(overlays.begin(), overlays.end(),
                                   [](const auto* overlay) { return overlay->isToolbarHost(); });
    REQUIRE(host != overlays.end());
    const auto passive = std::find_if(overlays.begin(), overlays.end(),
                                      [host](const auto* overlay) { return overlay != *host; });
    REQUIRE(passive != overlays.end());
    REQUIRE(session.document() != nullptr);
    REQUIRE(session.document()
                ->addObject(lc::annotation::TextAnnotation{{18, 3}, QStringLiteral("old"), {Qt::red, 12}})
                .has_value());

    QMouseEvent initialPress{QEvent::MouseButtonPress, QPointF{1, 1}, QPointF{1, 1},
                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(*passive, &initialPress);
    auto* editor = (*host)->findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(editor != nullptr);
    CHECK(editor->toPlainText().isEmpty());

    sendDoubleClickSequence(**passive, {3, 8});

    editor = (*host)->findChild<QPlainTextEdit*>("annotationTextEditor");
    REQUIRE(editor != nullptr);
    CHECK(editor->toPlainText() == QStringLiteral("old"));
    const auto editorCount = std::count_if(overlays.begin(), overlays.end(), [](const auto* overlay) {
        return overlay->template findChild<QPlainTextEdit*>("annotationTextEditor") != nullptr;
    });
    CHECK(editorCount == 1);
    session.cancel();
}
