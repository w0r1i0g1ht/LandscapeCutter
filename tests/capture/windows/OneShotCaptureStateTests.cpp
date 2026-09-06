#include "capture/windows/OneShotCaptureState.hpp"
#include "capture/windows/MonitorCaptureService.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>

namespace {
using namespace lc::capture;
using namespace lc::capture::windows;
using namespace lc::graphics::d3d11;
using Phase = OneShotCaptureState::Phase;

struct FakeSourceData {
    FrameHandler handler;
    CapturePoolOptions options{};
    int starts{};
    int subscriptions{};
    int sessions{};
    int pools{};
    bool supported{true};
    HRESULT startError{S_OK};
    std::function<void()> onRevoke;
    std::function<void()> onClose;
    std::vector<int> cleanupOrder;
};

// Only the external WGC producer is replaced. The service, completion gate,
// Qt timer/queue, real D3D textures and copy boundary remain production code.
class FakeSource final : public IMonitorFrameSource {
public:
    explicit FakeSource(std::shared_ptr<FakeSourceData> data) : data_(std::move(data)) {}
    bool isSupported() override { return data_->supported; }
    void start(HMONITOR, const D3dDeviceBundle&, const CapturePoolOptions& options,
               FrameHandler handler) override {
        ++data_->starts;
        data_->options = options;
        data_->handler = std::move(handler);
        data_->subscriptions = data_->sessions = data_->pools = 1;
        winrt::check_hresult(data_->startError);
    }
    void revoke() noexcept override {
        data_->subscriptions = 0;
        data_->handler = {};
        data_->cleanupOrder.push_back(1);
        if (data_->onRevoke) { data_->onRevoke(); }
    }
    void close() noexcept override {
        data_->sessions = data_->pools = 0;
        data_->cleanupOrder.push_back(2);
        if (data_->onClose) { data_->onClose(); }
    }
private:
    std::shared_ptr<FakeSourceData> data_;
};

class WarpFactory final : public ID3d11DeviceFactory {
    D3dCreateResult create(D3dDriverKind) override {
        return D3d11DeviceFactory{}.create(D3dDriverKind::Warp);
    }
};

MonitorCaptureRequest request(bool hdr = false) {
    MonitorCaptureRequest value{};
    value.monitor.nativeHandle = reinterpret_cast<HMONITOR>(1);
    value.monitor.id = {"fixture-monitor"};
    value.monitor.desktopRect = {-2, 0, 0, 2};
    value.monitor.hdrEnabled = hdr;
    value.monitor.catalogGeneration = 17;
    return value;
}

struct Fixture {
    D3d11DeviceManager manager{std::make_unique<WarpFactory>()};
    std::shared_ptr<FakeSourceData> source = std::make_shared<FakeSourceData>();
    std::unique_ptr<MonitorCaptureService> service;
    std::optional<CaptureResult> result;
    int completions{};

    Fixture() {
        REQUIRE(manager.initialize());
        service = std::make_unique<MonitorCaptureService>(manager, [data = source] {
            return std::make_unique<FakeSource>(data);
        });
    }
    void start(MonitorCaptureRequest input = request()) {
        service->captureOnce(std::move(input), [this](CaptureResult value) {
            CHECK(source->subscriptions == 0);
            CHECK(source->sessions == 0);
            CHECK(source->pools == 0);
            CHECK(service->findChildren<QTimer*>().empty());
            CHECK(QThread::currentThread() == QCoreApplication::instance()->thread());
            ++completions;
            result.emplace(std::move(value));
        });
    }
    IncomingCaptureFrame frame(DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM, UINT width = 2) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = 2;
        desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
        desc.Format = format;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        IncomingCaptureFrame value{};
        winrt::check_hresult(manager.current()->device->CreateTexture2D(
            &desc, nullptr, value.texture.put()));
        value.contentWidth = value.contentHeight = 2;
        value.systemRelativeTime = std::chrono::nanoseconds{12300};
        return value;
    }
    void pushFrame(IncomingCaptureFrame value) {
        auto handler = source->handler;
        handler([value] { return value; });
    }
    void drain() { QCoreApplication::processEvents(QEventLoop::AllEvents); }
    CaptureErrorCode error() { return std::get<CaptureError>(*result).code; }
};
}  // namespace

TEST_CASE("the first valid frame wins completion exactly once") {
    int count{};
    OneShotCaptureState state([&](CaptureResult) { ++count; });
    CHECK(state.phase() == Phase::AwaitingFrame);
    REQUIRE(state.tryBeginFrame());
    CHECK(state.phase() == Phase::ProcessingFrame);
    CHECK_FALSE(state.tryBeginFrame());
    CHECK_FALSE(state.timeout());
    REQUIRE(state.finishFrame(CaptureFrame{}));
    CHECK(state.phase() == Phase::Finalizing);
    CHECK(count == 0);
    state.completeAfterCleanup();
    state.completeAfterCleanup();
    CHECK(state.phase() == Phase::Completed);
    CHECK(count == 1);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<CaptureFrame>);
    STATIC_REQUIRE(std::is_move_constructible_v<CaptureFrame>);
}

TEST_CASE("timeout wins when no frame arrives and late frames cannot complete after timeout") {
    int count{};
    OneShotCaptureState state([&](CaptureResult value) {
        CHECK(std::get<CaptureError>(value).code == CaptureErrorCode::Timeout);
        ++count;
    });
    REQUIRE(state.timeout());
    CHECK_FALSE(state.timeout());
    CHECK_FALSE(state.tryBeginFrame());
    CHECK_FALSE(state.finishFrame(CaptureFrame{}));
    state.completeAfterCleanup();
    CHECK(state.phase() == Phase::Completed);
    CHECK(count == 1);
}

TEST_CASE("cancel suppresses UI completion during shutdown including queued and processing frames") {
    for (int stage = 0; stage < 3; ++stage) {
        int count{};
        OneShotCaptureState state([&](CaptureResult) { ++count; });
        if (stage > 0) { REQUIRE(state.tryBeginFrame()); }
        if (stage > 1) { REQUIRE(state.finishFrame(CaptureFrame{})); }
        state.cancel();
        state.cancel();
        CHECK_FALSE(state.finishFrame(CaptureFrame{}));
        CHECK_FALSE(state.timeout());
        state.completeAfterCleanup();
        CHECK(state.phase() == Phase::Cancelled);
        CHECK(count == 0);
    }
}

TEST_CASE("frame and timeout race through one atomic completion gate") {
    for (int i = 0; i < 100; ++i) {
        std::atomic<int> completed{};
        OneShotCaptureState state([&](CaptureResult) { ++completed; });
        std::thread frame([&] {
            if (state.tryBeginFrame()) { state.finishFrame(CaptureFrame{}); }
        });
        std::thread timeout([&] { state.timeout(); });
        frame.join(); timeout.join();
        state.completeAfterCleanup();
        CHECK(state.phase() == Phase::Completed);
        CHECK(completed == 1);
    }
}

TEST_CASE("capture service copies SDR and HDR frames before completion and closes its resources") {
    for (const bool hdr : {false, true}) {
        Fixture fixture;
        fixture.start(request(hdr));
        REQUIRE(fixture.source->starts == 1);
        CHECK(fixture.source->options.bufferCount == 2);
        CHECK(fixture.source->options.timeout == std::chrono::milliseconds{2000});
        CHECK(fixture.source->options.size.width == 2);
        CHECK(fixture.source->options.size.height == 2);
        CHECK(fixture.source->options.format ==
              (hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM));
        const auto timers = fixture.service->findChildren<QTimer*>();
        REQUIRE(timers.size() == 1);
        CHECK(timers.front()->interval() == 2000);
        CHECK(timers.front()->isSingleShot());
        auto frame = fixture.frame(hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM);
        auto* original = frame.texture.get();
        const auto handler = fixture.source->handler;
        std::thread producer([handler, frame] { handler([frame] { return frame; }); });
        producer.join();
        CHECK(fixture.completions == 0);
        fixture.drain();
        REQUIRE(fixture.result.has_value());
        REQUIRE(std::holds_alternative<CaptureFrame>(*fixture.result));
        const auto& owned = std::get<CaptureFrame>(*fixture.result);
        CHECK(owned.texture.get() != original);
        CHECK(owned.sourceMonitor.value == "fixture-monitor");
        CHECK(owned.displayGeneration == 17);
        CHECK(owned.deviceGeneration == 1);
        CHECK(owned.size.width == 2);
        CHECK(owned.size.height == 2);
        CHECK(owned.pixelFormat == (hdr ? CapturePixelFormat::Rgba16Float : CapturePixelFormat::Bgra8Unorm));
        CHECK(owned.systemRelativeTime == std::chrono::nanoseconds{12300});
        frame.texture = nullptr;
        D3D11_TEXTURE2D_DESC desc{};
        owned.texture->GetDesc(&desc);
        CHECK(desc.Width == 2);
        CHECK(desc.Height == 2);
        CHECK(desc.Format == (hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM));
        CHECK(fixture.source->cleanupOrder == std::vector<int>{1, 2});
        handler([]() -> IncomingCaptureFrame { FAIL_CHECK("late reader must not execute"); return {}; });
        fixture.drain();
        CHECK(fixture.completions == 1);
    }
}

TEST_CASE("empty frames fail completion exactly once") {
    Fixture fixture;
    fixture.start();
    fixture.pushFrame({});
    fixture.pushFrame({});
    fixture.drain();
    REQUIRE(fixture.result.has_value());
    CHECK(fixture.error() == CaptureErrorCode::EmptyFrame);
    CHECK(fixture.completions == 1);
}

TEST_CASE("content size and texture description mismatches are rejected") {
    for (int malformed = 0; malformed < 5; ++malformed) {
        Fixture fixture;
        fixture.start();
        auto value = fixture.frame(malformed == 3 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM,
                                   malformed == 4 ? 3U : 2U);
        if (malformed == 0) { value.contentWidth = 0; }
        if (malformed == 1) { value.contentWidth = -1; }
        if (malformed == 2) { value.contentWidth = 3; }
        fixture.pushFrame(value);
        fixture.drain();
        REQUIRE(fixture.result.has_value());
        CHECK(fixture.error() == (malformed < 2 ? CaptureErrorCode::EmptyFrame : CaptureErrorCode::DisplayChanged));
        CHECK(fixture.completions == 1);
    }
}

TEST_CASE("capture timeout shuts down before completion and rejects late frames") {
    Fixture fixture;
    fixture.start();
    auto late = fixture.source->handler;
    const auto timers = fixture.service->findChildren<QTimer*>();
    REQUIRE(timers.size() == 1);
    REQUIRE(QMetaObject::invokeMethod(timers.front(), "timeout", Qt::DirectConnection));
    late([]() -> IncomingCaptureFrame { FAIL_CHECK("timeout must reject late frame reads"); return {}; });
    fixture.drain();
    REQUIRE(fixture.result.has_value());
    CHECK(fixture.error() == CaptureErrorCode::Timeout);
    CHECK(fixture.completions == 1);
}

TEST_CASE("cancel drains active callbacks before closing and releases all resources in 100 cycles") {
    Fixture fixture;
    for (int i = 0; i < 100; ++i) {
        auto alive = std::make_shared<int>(i);
        std::weak_ptr<int> completionLifetime = alive;
        fixture.service->captureOnce(request(), [alive](CaptureResult) { FAIL_CHECK("cancelled completion"); });
        alive.reset();
        const auto late = fixture.source->handler;
        if (i % 2) { fixture.pushFrame(fixture.frame()); }
        fixture.service->cancel();
        fixture.drain();
        late([]() -> IncomingCaptureFrame { FAIL_CHECK("cancelled reader"); return {}; });
        CHECK(completionLifetime.expired());
        CHECK(fixture.service->findChildren<QTimer*>().empty());
        CHECK(fixture.source->subscriptions == 0);
        CHECK(fixture.source->sessions == 0);
        CHECK(fixture.source->pools == 0);
    }
    for (int i = 0; i < 100; ++i) {
        OneShotCaptureState state([](CaptureResult) { FAIL_CHECK("shutdown completion"); });
        REQUIRE(state.enterCallback());
        CHECK(state.activeCallbacks() == 1);
        state.preventCallbacks();
        CHECK_FALSE(state.enterCallback());
        std::thread callback([&] { state.leaveCallback(); });
        state.waitForCallbacks();
        callback.join();
        state.cancel();
        CHECK(state.activeCallbacks() == 0);
        CHECK(state.phase() == Phase::Cancelled);
    }
}

TEST_CASE("capture destructor suppresses already queued completion") {
    Fixture fixture;
    fixture.start();
    const auto late = fixture.source->handler;
    fixture.pushFrame(fixture.frame());
    fixture.service.reset();
    late([]() -> IncomingCaptureFrame { FAIL_CHECK("destroyed service reader"); return {}; });
    fixture.drain();
    CHECK(fixture.completions == 0);
    CHECK(fixture.source->sessions == 0);
    CHECK(fixture.source->subscriptions == 0);
}

TEST_CASE("capture errors are mapped before Qt completion and partial sessions are closed") {
    for (const auto& [nativeCode, expected] : std::vector<std::pair<HRESULT, CaptureErrorCode>>{
             {E_ACCESSDENIED, CaptureErrorCode::AccessDenied},
             {DXGI_ERROR_DEVICE_REMOVED, CaptureErrorCode::DeviceLost},
             {E_INVALIDARG, CaptureErrorCode::MonitorUnavailable},
             {E_FAIL, CaptureErrorCode::Internal}}) {
        Fixture fixture;
        fixture.source->startError = nativeCode;
        CHECK_NOTHROW(fixture.start());
        fixture.drain();
        REQUIRE(fixture.result.has_value());
        CHECK(fixture.error() == expected);
        CHECK(std::get<CaptureError>(*fixture.result).nativeCode.value() == nativeCode);
        CHECK(fixture.completions == 1);
    }
}

TEST_CASE("capture unsupported and missing device fail without starting a session") {
    Fixture fixture;
    fixture.source->supported = false;
    fixture.start(); fixture.drain();
    REQUIRE(fixture.result.has_value());
    CHECK(fixture.error() == CaptureErrorCode::Unsupported);
    CHECK(fixture.source->starts == 0);
    D3d11DeviceManager missing;
    MonitorCaptureService service(missing, [data = fixture.source] { return std::make_unique<FakeSource>(data); });
    fixture.source->supported = true;
    int count{};
    service.captureOnce(request(), [&](CaptureResult value) {
        CHECK(std::get<CaptureError>(value).code == CaptureErrorCode::DeviceCreationFailed);
        ++count;
    });
    fixture.drain();
    CHECK(count == 1);
    CHECK(fixture.source->starts == 0);
}

TEST_CASE("capture duplicate requests neither start nor queue a second session") {
    Fixture fixture;
    fixture.start();
    int rejected{};
    fixture.service->captureOnce(request(), [&](CaptureResult value) {
        CHECK(std::get<CaptureError>(value).code == CaptureErrorCode::Cancelled);
        ++rejected;
    });
    fixture.drain();
    CHECK(rejected == 1);
    CHECK(fixture.source->starts == 1);
    fixture.pushFrame(fixture.frame()); fixture.drain();
    CHECK(fixture.completions == 1);
    CHECK(fixture.source->starts == 1);
}

TEST_CASE("capture cancel waits for an executing frame reader before closing its source") {
    Fixture fixture;
    fixture.start();
    std::promise<void> entered;
    auto enteredFuture = entered.get_future();
    std::promise<void> revoked;
    auto revokedFuture = revoked.get_future();
    std::atomic<int> activeReaders{};
    std::atomic<bool> closedWhileReading{};
    fixture.source->onRevoke = [&] { revoked.set_value(); };
    fixture.source->onClose = [&] { closedWhileReading = activeReaders != 0; };
    auto handler = fixture.source->handler;
    std::thread producer([&] {
        handler([&] {
            ++activeReaders;
            entered.set_value();
            revokedFuture.wait();
            --activeReaders;
            return IncomingCaptureFrame{};
        });
    });
    enteredFuture.wait();
    fixture.service->cancel();
    producer.join();
    fixture.drain();
    CHECK_FALSE(closedWhileReading);
    CHECK(activeReaders == 0);
    CHECK(fixture.completions == 0);
    CHECK(fixture.service->findChildren<QTimer*>().empty());
}

TEST_CASE("capture borrowed frame is released before queued completion") {
    Fixture fixture;
    fixture.start();
    auto frame = fixture.frame();
    auto lease = std::make_shared<int>(1);
    std::weak_ptr<int> borrowedLifetime = lease;
    frame.lifetime = std::move(lease);
    fixture.pushFrame(std::move(frame));
    CHECK(borrowedLifetime.expired());
    fixture.drain();
    CHECK(fixture.completions == 1);
    CHECK(std::get<CaptureFrame>(*fixture.result).texture);
}

TEST_CASE("capture frame reader HRESULT errors cannot escape the callback boundary") {
    Fixture fixture;
    fixture.start();
    const auto handler = fixture.source->handler;
    CHECK_NOTHROW(handler([]() -> IncomingCaptureFrame { winrt::throw_hresult(E_ACCESSDENIED); }));
    fixture.drain();
    REQUIRE(fixture.result.has_value());
    CHECK(fixture.error() == CaptureErrorCode::AccessDenied);
    CHECK(fixture.completions == 1);
}

TEST_CASE("capture completion can destroy the service after all resources close") {
    Fixture fixture;
    int completions{};
    fixture.service->captureOnce(request(), [&](CaptureResult result) {
        CHECK(std::holds_alternative<CaptureFrame>(result));
        CHECK(fixture.source->sessions == 0);
        CHECK(fixture.source->subscriptions == 0);
        fixture.service.reset();
        ++completions;
    });
    fixture.pushFrame(fixture.frame());
    fixture.drain();
    CHECK(completions == 1);
    CHECK_FALSE(fixture.service);
}

TEST_CASE("capture cancel suppresses queued duplicate rejection callbacks") {
    Fixture fixture;
    fixture.start();
    int rejected{};
    auto lifetime = std::make_shared<int>(1);
    std::weak_ptr<int> retained = lifetime;
    fixture.service->captureOnce(request(), [&, lifetime](CaptureResult) { ++rejected; });
    lifetime.reset();
    fixture.service->cancel();
    CHECK(retained.expired());
    fixture.drain();
    CHECK(rejected == 0);
    CHECK(fixture.completions == 0);
}
