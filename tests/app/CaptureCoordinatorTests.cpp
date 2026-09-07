#include "app/CaptureCoordinator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using namespace lc;
using namespace lc::app;
using namespace lc::capture;
using namespace lc::graphics::d3d11;
using namespace lc::platform;
using namespace lc::platform::windows;

MonitorDescriptor monitor(std::string id = "display-a", std::uint64_t generation = 7) {
    MonitorDescriptor value{};
    value.id = {std::move(id)};
    value.displayName = L"Fixture display";
    value.catalogGeneration = generation;
    return value;
}

CaptureFrame frame(const MonitorDescriptor& source, std::uint64_t deviceGeneration = 3) {
    CaptureFrame value{};
    value.size = {1920, 1080};
    value.pixelFormat = CapturePixelFormat::Bgra8Unorm;
    value.sourceMonitor = source.id;
    value.displayGeneration = source.catalogGeneration;
    value.deviceGeneration = deviceGeneration;
    return value;
}

class FakeCaptureService final : public IMonitorCaptureService {
public:
    void captureOnce(MonitorCaptureRequest request, CaptureCompletion completion) override {
        requests.push_back(std::move(request));
        completions.push_back(std::move(completion));
    }

    void cancel() noexcept override { ++cancelCount; }

    void complete(std::size_t index, CaptureResult result) {
        completions.at(index)(std::move(result));
    }

    std::vector<MonitorCaptureRequest> requests;
    std::vector<CaptureCompletion> completions;
    int cancelCount{};
};

class FakeDeviceRecovery final : public ID3d11DeviceRecovery {
public:
    bool rebuild() override {
        ++rebuildCount;
        if (rebuildSucceeds) { ++deviceGeneration; }
        return rebuildSucceeds;
    }

    std::uint64_t generation() const noexcept override { return deviceGeneration; }

    bool rebuildSucceeds{true};
    std::uint64_t deviceGeneration{3};
    int rebuildCount{};
};

struct Fixture {
    FakeCaptureService capture;
    FakeDeviceRecovery recovery;
    std::optional<MonitorDescriptor> selected{monitor()};
    CaptureCoordinator coordinator{capture, recovery, [this] { return selected; }};
    std::vector<CaptureNotice> notices;
    std::vector<CaptureAvailability> availability;

    Fixture() {
        QObject::connect(&coordinator, &CaptureCoordinator::noticeReady,
                         [this](const CaptureNotice& notice) { notices.push_back(notice); });
        QObject::connect(&coordinator, &CaptureCoordinator::availabilityChanged,
                         [this](CaptureAvailability value) { availability.push_back(value); });
    }
};

TEST_CASE("a capture request keeps the monitor selected when it started") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    REQUIRE(fixture.capture.requests.size() == 1);
    CHECK(fixture.capture.requests.front().monitor.id.value == "display-a");

    fixture.selected = monitor("display-b", 7);
    fixture.capture.complete(0, frame(monitor()));

    REQUIRE(fixture.notices.size() == 1);
    CHECK(fixture.notices.front().code == CaptureNoticeCode::Success);
    CHECK(fixture.notices.front().monitor->value == "display-a");
    CHECK(fixture.coordinator.latestFrame()->sourceMonitor.value == "display-a");
}

TEST_CASE("a second request while capturing reports busy and does not change the active result") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.coordinator.requestCapture();

    CHECK(fixture.coordinator.state() == CaptureState::Capturing);
    REQUIRE(fixture.notices.size() == 1);
    CHECK(fixture.notices.front().code == CaptureNoticeCode::Busy);
    REQUIRE(fixture.capture.requests.size() == 1);

    fixture.capture.complete(0, frame(monitor()));
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.coordinator.latestFrame().has_value());
}

TEST_CASE("a successful capture replaces the latest frame and reports its output") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    REQUIRE(fixture.coordinator.latestFrame().has_value());
    const auto& latest = *fixture.coordinator.latestFrame();
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    CHECK(latest.size.width == 1920);
    CHECK(latest.size.height == 1080);
    CHECK(latest.pixelFormat == CapturePixelFormat::Bgra8Unorm);
    REQUIRE(fixture.notices.size() == 1);
    CHECK(fixture.notices.front().code == CaptureNoticeCode::Success);
    CHECK(fixture.notices.front().displayName == L"Fixture display");
    REQUIRE(fixture.notices.front().size.has_value());
    CHECK(fixture.notices.front().size->width == 1920);
    CHECK(fixture.notices.front().size->height == 1080);
    CHECK(fixture.notices.front().pixelFormat == CapturePixelFormat::Bgra8Unorm);
}

TEST_CASE("a later success replaces the complete latest frame") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    auto replacement = frame(monitor());
    replacement.size = {1280, 720};
    replacement.pixelFormat = CapturePixelFormat::Rgba16Float;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(1, std::move(replacement));

    REQUIRE(fixture.coordinator.latestFrame().has_value());
    CHECK(fixture.coordinator.latestFrame()->size.width == 1280);
    CHECK(fixture.coordinator.latestFrame()->size.height == 720);
    CHECK(fixture.coordinator.latestFrame()->pixelFormat == CapturePixelFormat::Rgba16Float);
    REQUIRE(fixture.notices.size() == 2);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::Success);
    CHECK(fixture.notices.back().size->width == 1280);
    CHECK(fixture.notices.back().pixelFormat == CapturePixelFormat::Rgba16Float);
}

TEST_CASE("a failed capture preserves the most recent valid frame") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));
    fixture.coordinator.requestCapture();
    fixture.capture.complete(1, CaptureError{CaptureErrorCode::Timeout, {}});

    REQUIRE(fixture.coordinator.latestFrame().has_value());
    CHECK(fixture.coordinator.latestFrame()->size.width == 1920);
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.notices.size() == 2);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::Timeout);
}

TEST_CASE("a changed current catalog generation rejects a result that echoes the request generation") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    fixture.coordinator.requestCapture();
    fixture.selected = monitor("display-a", 8);
    fixture.capture.complete(1, frame(monitor()));
    CHECK(fixture.coordinator.latestFrame()->displayGeneration == 7);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DisplayChanged);
}

TEST_CASE("a display change still releases a stale device frame") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    fixture.coordinator.requestCapture();
    fixture.selected = monitor("display-a", 8);
    fixture.recovery.deviceGeneration = 4;
    fixture.capture.complete(1, CaptureError{CaptureErrorCode::DeviceLost, {}});

    CHECK_FALSE(fixture.coordinator.latestFrame().has_value());
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.notices.size() == 2);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DisplayChanged);
    CHECK(fixture.availability.empty());
    CHECK(fixture.recovery.rebuildCount == 0);
}

TEST_CASE("a result with a stale display generation preserves the latest frame") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    auto staleDisplay = frame(monitor());
    staleDisplay.displayGeneration = 6;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(1, std::move(staleDisplay));

    REQUIRE(fixture.coordinator.latestFrame().has_value());
    CHECK(fixture.coordinator.latestFrame()->displayGeneration == 7);
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.notices.size() == 2);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DisplayChanged);
    CHECK(fixture.availability.empty());
}

TEST_CASE("an unavailable current catalog rejects an error before device recovery") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.selected.reset();
    fixture.capture.complete(0, CaptureError{CaptureErrorCode::DeviceLost, {}});

    CHECK(fixture.recovery.rebuildCount == 0);
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.notices.size() == 1);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DisplayUnavailable);
    REQUIRE(fixture.availability.size() == 1);
    CHECK(fixture.availability.back() == CaptureAvailability::DisplayUnavailable);
}

TEST_CASE("a stale device generation does not replace the current frame") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    fixture.coordinator.requestCapture();
    auto staleDevice = frame(monitor());
    staleDevice.deviceGeneration = 2;
    fixture.capture.complete(1, std::move(staleDevice));
    CHECK(fixture.coordinator.latestFrame()->deviceGeneration == 3);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DeviceUnavailable);
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
}

TEST_CASE("a current device generation drift releases the latest frame") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    fixture.coordinator.requestCapture();
    fixture.recovery.deviceGeneration = 4;
    fixture.capture.complete(1, frame(monitor()));

    CHECK_FALSE(fixture.coordinator.latestFrame().has_value());
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.notices.size() == 2);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DeviceUnavailable);
    REQUIRE(fixture.availability.size() == 1);
    CHECK(fixture.availability.back() == CaptureAvailability::DeviceUnavailable);
}

TEST_CASE("one device loss rebuilds once, clears the old frame, and retries the selected monitor") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));

    fixture.coordinator.requestCapture();
    fixture.selected = monitor("display-b", 7);
    fixture.capture.complete(1, CaptureError{CaptureErrorCode::DeviceLost, {}});
    CHECK_FALSE(fixture.coordinator.latestFrame().has_value());
    CHECK(fixture.recovery.rebuildCount == 1);
    REQUIRE(fixture.capture.requests.size() == 3);
    CHECK(fixture.capture.requests.back().monitor.id.value == "display-a");
    CHECK(fixture.coordinator.state() == CaptureState::Capturing);

    fixture.capture.complete(2, frame(monitor(), 4));
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.coordinator.latestFrame().has_value());
    CHECK(fixture.notices.back().code == CaptureNoticeCode::Success);
}

TEST_CASE("a second device loss stops retrying and disables capture") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, CaptureError{CaptureErrorCode::DeviceLost, {}});
    fixture.capture.complete(1, CaptureError{CaptureErrorCode::DeviceLost, {}});

    CHECK(fixture.recovery.rebuildCount == 1);
    CHECK(fixture.capture.requests.size() == 2);
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.notices.size() == 1);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DeviceUnavailable);
    REQUIRE(fixture.availability.size() == 1);
    CHECK(fixture.availability.back() == CaptureAvailability::DeviceUnavailable);
}

TEST_CASE("a failed device rebuild reports recovery failure and disables capture") {
    Fixture fixture;
    fixture.recovery.rebuildSucceeds = false;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, CaptureError{CaptureErrorCode::DeviceLost, {}});

    CHECK(fixture.recovery.rebuildCount == 1);
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    REQUIRE(fixture.notices.size() == 1);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DeviceRecoveryFailed);
    REQUIRE(fixture.availability.size() == 1);
    CHECK(fixture.availability.back() == CaptureAvailability::DeviceUnavailable);
}

TEST_CASE("each terminal capture error maps to its structured notice") {
    const std::vector<std::pair<CaptureErrorCode, CaptureNoticeCode>> cases{
        {CaptureErrorCode::Unsupported, CaptureNoticeCode::Unsupported},
        {CaptureErrorCode::MonitorUnavailable, CaptureNoticeCode::MonitorUnavailable},
        {CaptureErrorCode::AccessDenied, CaptureNoticeCode::AccessDenied},
        {CaptureErrorCode::Timeout, CaptureNoticeCode::Timeout},
        {CaptureErrorCode::EmptyFrame, CaptureNoticeCode::InvalidFrame},
        {CaptureErrorCode::DisplayChanged, CaptureNoticeCode::DisplayChanged},
        {CaptureErrorCode::DeviceCreationFailed, CaptureNoticeCode::DeviceUnavailable},
        {CaptureErrorCode::Cancelled, CaptureNoticeCode::CaptureCancelled},
        {CaptureErrorCode::Internal, CaptureNoticeCode::InternalFailure},
    };

    for (const auto& [error, expected] : cases) {
        Fixture fixture;
        fixture.coordinator.requestCapture();
        fixture.capture.complete(0, CaptureError{error, {}});
        REQUIRE(fixture.notices.size() == 1);
        CHECK(fixture.notices.front().code == expected);
        CHECK(fixture.coordinator.state() == CaptureState::Idle);
    }
}

TEST_CASE("availability blocks capture and reports the matching condition") {
    const std::vector<std::pair<CaptureAvailability, CaptureNoticeCode>> cases{
        {CaptureAvailability::Unsupported, CaptureNoticeCode::Unsupported},
        {CaptureAvailability::DisplayUnavailable, CaptureNoticeCode::DisplayUnavailable},
        {CaptureAvailability::DeviceUnavailable, CaptureNoticeCode::DeviceUnavailable},
    };

    for (const auto& [availability, expected] : cases) {
        Fixture fixture;
        fixture.coordinator.setAvailability(availability);
        fixture.coordinator.requestCapture();
        CHECK(fixture.capture.requests.empty());
        REQUIRE(fixture.notices.size() == 1);
        CHECK(fixture.notices.front().code == expected);
        REQUIRE(fixture.availability.size() == 1);
        CHECK(fixture.availability.front() == availability);
    }
}

TEST_CASE("a terminal availability failure is visible before a notice listener can request again") {
    Fixture fixture;
    bool reentered{};
    QObject::connect(&fixture.coordinator, &CaptureCoordinator::noticeReady,
                     [&fixture, &reentered](const CaptureNotice& notice) {
                         if (notice.code == CaptureNoticeCode::DeviceUnavailable && !reentered) {
                             reentered = true;
                             fixture.coordinator.requestCapture();
                         }
                     });
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, CaptureError{CaptureErrorCode::DeviceCreationFailed, {}});

    CHECK(reentered);
    CHECK(fixture.capture.requests.size() == 1);
    REQUIRE(fixture.notices.size() == 2);
    CHECK(fixture.notices.back().code == CaptureNoticeCode::DeviceUnavailable);
}

TEST_CASE("shutdown cancels an active request, releases the latest frame, and ignores late completion") {
    Fixture fixture;
    fixture.coordinator.requestCapture();
    fixture.capture.complete(0, frame(monitor()));
    fixture.coordinator.requestCapture();
    fixture.coordinator.shutdown();
    fixture.capture.complete(1, frame(monitor()));

    CHECK(fixture.capture.cancelCount == 1);
    CHECK(fixture.coordinator.state() == CaptureState::Idle);
    CHECK_FALSE(fixture.coordinator.latestFrame().has_value());
    REQUIRE(fixture.notices.size() == 1);
    CHECK(fixture.notices.front().code == CaptureNoticeCode::Success);
}

TEST_CASE("a retained completion after coordinator destruction is ignored") {
    FakeCaptureService capture;
    FakeDeviceRecovery recovery;
    std::optional<MonitorDescriptor> selected{monitor()};
    int notices{};
    auto coordinator = std::make_unique<CaptureCoordinator>(capture, recovery,
                                                             [&selected] { return selected; });
    QObject::connect(coordinator.get(), &CaptureCoordinator::noticeReady,
                     [&notices](const CaptureNotice&) { ++notices; });
    coordinator->requestCapture();
    REQUIRE(capture.completions.size() == 1);

    coordinator.reset();
    capture.complete(0, frame(monitor()));

    CHECK(capture.cancelCount == 1);
    CHECK(notices == 0);
}
}  // namespace
