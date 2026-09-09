#include "snip/SnapshotBatch.hpp"
#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>
using namespace lc;
namespace {
struct Service : capture::IMonitorCaptureService {
    std::vector<capture::CaptureCompletion> pending;
    void captureOnce(capture::MonitorCaptureRequest, capture::CaptureCompletion done) override {
        pending.push_back(std::move(done));
    }
    void cancel() noexcept override {}
};
struct Recovery : graphics::d3d11::ID3d11DeviceRecovery {
    int rebuilds = 0;
    bool works = true;
    std::uint64_t gen = 1;
    bool rebuild() override {
        ++rebuilds;
        if (works)
            ++gen;
        return works;
    }
    std::uint64_t generation() const noexcept override {
        return gen;
    }
};
std::vector<platform::windows::MonitorDescriptor> monitors() {
    platform::windows::MonitorDescriptor a{}, b{};
    a.id = {"a"};
    a.desktopRect = {-2, 0, 0, 2};
    a.catalogGeneration = 1;
    b.id = {"b"};
    b.desktopRect = {0, 0, 2, 2};
    b.catalogGeneration = 1;
    return {a, b};
}
capture::CaptureFrame frame(const char* id, std::uint64_t gen = 1) {
    capture::CaptureFrame f;
    f.sourceMonitor = {id};
    f.size = {2, 2};
    f.displayGeneration = 1;
    f.deviceGeneration = gen;
    return f;
}
snip::ReadbackFunction reader = [](capture::CaptureFrame, snip::ReadbackCompletion done) {
    QImage img(2, 2, QImage::Format_RGB32);
    img.fill(Qt::red);
    done(img);
};
} // namespace
TEST_CASE("snip batch publishes only after every monitor is frozen") {
    int argc = 1;
    char name[] = "test";
    char* argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    Service service;
    Recovery recovery;
    snip::SnapshotBatch batch(service, recovery, reader);
    int ready = 0;
    std::vector<snip::FrozenMonitor> result;
    QObject::connect(&batch, &snip::SnapshotBatch::ready, [&](auto images) {
        ++ready;
        result = std::move(images);
    });
    batch.start(monitors());
    batch.start(monitors());
    REQUIRE(service.pending.size() == 1);
    auto first = service.pending[0];
    first(frame("a"));
    CHECK(ready == 0);
    REQUIRE(service.pending.size() == 2);
    auto second = service.pending[1];
    second(frame("b"));
    REQUIRE(ready == 1);
    REQUIRE(result.size() == 2);
    CHECK(result[0].geometry == QRect(-2, 0, 2, 2));
    CHECK_FALSE(batch.active());
}
TEST_CASE("snip batch cancellation and failure discard pending images") {
    int argc = 1;
    char name[] = "test";
    char* argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    Service service;
    Recovery recovery;
    snip::SnapshotBatch batch(service, recovery, reader);
    int ready = 0, failed = 0;
    QObject::connect(&batch, &snip::SnapshotBatch::ready, [&](auto) { ++ready; });
    QObject::connect(&batch, &snip::SnapshotBatch::failed, [&](auto) { ++failed; });
    batch.start(monitors());
    auto old = service.pending[0];
    batch.cancel();
    old(frame("a"));
    CHECK(ready == 0);
    CHECK(service.pending.size() == 1);
    batch.start(monitors());
    auto current = service.pending[1];
    current(capture::CaptureError{capture::CaptureErrorCode::Timeout, {}});
    CHECK(failed == 1);
    CHECK(ready == 0);
    CHECK_FALSE(batch.active());
}
TEST_CASE("snip batch restarts all monitors once after device loss") {
    int argc = 1;
    char name[] = "test";
    char* argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    Service service;
    Recovery recovery;
    snip::SnapshotBatch batch(service, recovery, reader);
    int ready = 0, failed = 0;
    QObject::connect(&batch, &snip::SnapshotBatch::ready, [&](auto) { ++ready; });
    QObject::connect(&batch, &snip::SnapshotBatch::failed, [&](auto) { ++failed; });
    batch.start(monitors());
    auto a = service.pending[0];
    a(frame("a"));
    auto b = service.pending[1];
    b(capture::CaptureError{capture::CaptureErrorCode::DeviceLost, {}});
    REQUIRE(recovery.rebuilds == 1);
    REQUIRE(service.pending.size() == 3);
    auto retry = service.pending[2];
    retry(capture::CaptureError{capture::CaptureErrorCode::DeviceLost, {}});
    CHECK(recovery.rebuilds == 1);
    CHECK(failed == 1);
    CHECK(ready == 0);
}
TEST_CASE("snip batch rejects stale frame identity") {
    int argc = 1;
    char name[] = "test";
    char* argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    Service service;
    Recovery recovery;
    snip::SnapshotBatch batch(service, recovery, reader);
    int ready = 0, failed = 0;
    QObject::connect(&batch, &snip::SnapshotBatch::ready, [&](auto) { ++ready; });
    QObject::connect(&batch, &snip::SnapshotBatch::failed, [&](auto) { ++failed; });
    batch.start(monitors());
    auto a = service.pending[0];
    a(frame("wrong"));
    CHECK(failed == 1);
    CHECK(ready == 0);
    CHECK_FALSE(batch.active());
}

TEST_CASE("snip batch ignores readback completion after cancellation") {
    int argc = 1;
    char name[] = "test";
    char* argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    Service service;
    Recovery recovery;
    snip::ReadbackCompletion pendingReadback;
    snip::SnapshotBatch batch(
        service, recovery,
        [&pendingReadback](capture::CaptureFrame, snip::ReadbackCompletion done) {
            pendingReadback = std::move(done);
        });
    int ready = 0, failed = 0;
    QObject::connect(&batch, &snip::SnapshotBatch::ready, [&](auto) { ++ready; });
    QObject::connect(&batch, &snip::SnapshotBatch::failed, [&](auto) { ++failed; });

    batch.start(monitors());
    service.pending.front()(frame("a"));
    REQUIRE(static_cast<bool>(pendingReadback));
    batch.cancel();
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::red);
    pendingReadback(image);

    CHECK(ready == 0);
    CHECK(failed == 0);
    CHECK_FALSE(batch.active());
}
