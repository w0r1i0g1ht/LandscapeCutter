#include "capture/windows/MonitorCaptureService.hpp"
#include "graphics/d3d11/TextureCopy.hpp"
#include "platform/windows/DisplayCatalog.hpp"
#include "platform/windows/DpiAwareness.hpp"
#include "platform/windows/WindowsDisplayTopologySource.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <optional>

namespace {
using namespace lc;

class DesktopCaptureFixture final {
public:
    DesktopCaptureFixture() : catalog_(source_), copy_(manager_), service_(manager_) {
        REQUIRE(manager_.initialize());
    }

    std::vector<platform::windows::MonitorDescriptor> refreshMonitors() {
        auto result = catalog_.refresh();
        REQUIRE(std::holds_alternative<std::vector<platform::windows::MonitorDescriptor>>(result));
        auto monitors = std::get<std::vector<platform::windows::MonitorDescriptor>>(std::move(result));
        REQUIRE_FALSE(monitors.empty());
        std::cout << "active-monitors=" << monitors.size() << '\n';
        return monitors;
    }

    capture::CaptureFrame captureOnce(const platform::windows::MonitorDescriptor& monitor) {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        std::optional<capture::CaptureFrame> frame;
        std::optional<capture::CaptureError> error;
        bool expired = false;
        QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
            expired = true;
            // Cancellation must precede failure so WGC callbacks are drained.
            service_.cancel();
            loop.quit();
        });
        const auto started = std::chrono::steady_clock::now();
        deadline.start(2500);
        service_.captureOnce({monitor, std::chrono::milliseconds{2000}},
            [&](capture::CaptureResult value) {
                if (auto* captured = std::get_if<capture::CaptureFrame>(&value)) {
                    frame.emplace(std::move(*captured));
                } else {
                    error.emplace(std::get<capture::CaptureError>(std::move(value)));
                }
                loop.quit();
            });
        if (!frame && !error) { loop.exec(); }
        deadline.stop();
        // The service returns an owned frame only after closing the WGC session;
        // cancellation drains any remaining callback before local state expires.
        service_.cancel();
        REQUIRE_FALSE(expired);
        REQUIRE_FALSE(error.has_value());
        REQUIRE(frame.has_value());
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        std::cout << "physical-width=" << platform::width(monitor.desktopRect)
                  << " physical-height=" << platform::height(monitor.desktopRect)
                  << " dpi=" << monitor.dpiX << 'x' << monitor.dpiY
                  << " hdr=" << monitor.hdrEnabled << " elapsed-ms=" << elapsed << '\n';
        return std::move(*frame);
    }

    auto readback(ID3D11Texture2D& texture) { return copy_.readback(texture); }
    std::uint64_t deviceGeneration() const { return manager_.generation(); }

private:
    platform::windows::WindowsDisplayTopologySource source_;
    platform::windows::DisplayCatalog catalog_;
    graphics::d3d11::D3d11DeviceManager manager_;
    graphics::d3d11::TextureCopy copy_;
    capture::windows::MonitorCaptureService service_;
};
} // namespace

TEST_CASE("every active monitor produces one owned GPU frame") {
    DesktopCaptureFixture fixture;
    for (const auto& monitor : fixture.refreshMonitors()) {
        auto frame = fixture.captureOnce(monitor);
        REQUIRE(frame.texture);
        D3D11_TEXTURE2D_DESC desc{};
        frame.texture->GetDesc(&desc);
        CHECK(desc.Width == frame.size.width);
        CHECK(desc.Height == frame.size.height);
        CHECK(frame.sourceMonitor == monitor.id);
        CHECK(frame.displayGeneration == monitor.catalogGeneration);
        CHECK(frame.deviceGeneration == fixture.deviceGeneration());
        CHECK(frame.pixelFormat == (monitor.hdrEnabled ? capture::CapturePixelFormat::Rgba16Float
                                                       : capture::CapturePixelFormat::Bgra8Unorm));
        CHECK(desc.Format == (monitor.hdrEnabled ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                                : DXGI_FORMAT_B8G8R8A8_UNORM));
        CHECK(desc.Usage == D3D11_USAGE_DEFAULT);
        CHECK(desc.CPUAccessFlags == 0);
    }
}

TEST_CASE("captured textures survive WGC session shutdown and controlled readback") {
    DesktopCaptureFixture fixture;
    for (const auto& monitor : fixture.refreshMonitors()) {
        auto frame = fixture.captureOnce(monitor);
        REQUIRE(frame.texture);
        auto result = fixture.readback(*frame.texture);
        REQUIRE(std::holds_alternative<graphics::d3d11::TextureReadback>(result));
        const auto& readback = std::get<graphics::d3d11::TextureReadback>(result);
        CHECK(readback.size.width == frame.size.width);
        CHECK(readback.size.height == frame.size.height);
        CHECK(readback.rowPitch >= static_cast<std::size_t>(frame.size.width) * (monitor.hdrEnabled ? 8 : 4));
        CHECK(readback.bytes.size() == readback.rowPitch * static_cast<std::size_t>(frame.size.height));
        CHECK(readback.format == (monitor.hdrEnabled ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                                   : DXGI_FORMAT_B8G8R8A8_UNORM));
    }
}

int main(int argc, char* argv[]) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (platform::windows::ensurePerMonitorV2().status == platform::windows::DpiSetupStatus::Failed) {
        return 1;
    }
    QCoreApplication application(argc, argv);
    return Catch::Session().run(argc, argv);
}
