#include "snip/SnipOverlay.hpp"
#include "snip/SnipSession.hpp"
#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
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
    selectRect(session, {4, 4}, {16, 12});
    preparation.complete(QImage(10, 7, QImage::Format_RGB32));
    app.processEvents();

    REQUIRE(session.document() != nullptr);
    CHECK(session.document()->snapshot().base.size() == QSize(10, 7));
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

    session.beginAnnotation(lc::annotation::AnnotationTool::Rectangle);
    preparation.complete({});
    app.processEvents();

    CHECK(session.state() == lc::snip::SnipSessionState::Selecting);
    CHECK(session.selection().rect() == selection);
    CHECK(session.document() == nullptr);
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
