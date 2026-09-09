#include "capture/windows/MonitorCaptureService.hpp"
#include "graphics/d3d11/TextureCopy.hpp"
#include "platform/windows/DisplayCatalog.hpp"
#include "platform/windows/DpiAwareness.hpp"
#include "platform/windows/WindowsDisplayTopologySource.hpp"
#include "snip/SelectionModel.hpp"
#include "snip/SnapshotReadback.hpp"
#include "snip/SnipOverlay.hpp"

#include <QApplication>
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
        auto monitors =
            std::get<std::vector<platform::windows::MonitorDescriptor>>(std::move(result));
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
        service_.captureOnce(
            {monitor, std::chrono::milliseconds{2000}}, [&](capture::CaptureResult value) {
                if (auto* captured = std::get_if<capture::CaptureFrame>(&value)) {
                    frame.emplace(std::move(*captured));
                } else {
                    error.emplace(std::get<capture::CaptureError>(std::move(value)));
                }
                loop.quit();
            });
        if (!frame && !error) {
            loop.exec();
        }
        deadline.stop();
        // The service returns an owned frame only after closing the WGC session;
        // cancellation drains any remaining callback before local state expires.
        service_.cancel();
        REQUIRE_FALSE(expired);
        REQUIRE_FALSE(error.has_value());
        REQUIRE(frame.has_value());
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        std::cout << "physical-width=" << platform::width(monitor.desktopRect)
                  << " physical-height=" << platform::height(monitor.desktopRect)
                  << " dpi=" << monitor.dpiX << 'x' << monitor.dpiY << " hdr=" << monitor.hdrEnabled
                  << " elapsed-ms=" << elapsed << '\n';
        return std::move(*frame);
    }

    auto readback(ID3D11Texture2D& texture) {
        return copy_.readback(texture);
    }
    std::uint64_t deviceGeneration() const {
        return manager_.generation();
    }

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
        CHECK(desc.Format ==
              (monitor.hdrEnabled ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM));
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
        CHECK(readback.rowPitch >=
              static_cast<std::size_t>(frame.size.width) * (monitor.hdrEnabled ? 8 : 4));
        CHECK(readback.bytes.size() ==
              readback.rowPitch * static_cast<std::size_t>(frame.size.height));
        CHECK(readback.format ==
              (monitor.hdrEnabled ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM));
    }
}

TEST_CASE("all desktop snapshots pass worker readback before publication") {
    platform::windows::WindowsDisplayTopologySource source;
    platform::windows::DisplayCatalog catalog(source);
    REQUIRE(std::holds_alternative<std::vector<platform::windows::MonitorDescriptor>>(
        catalog.refresh()));
    graphics::d3d11::D3d11DeviceManager manager;
    REQUIRE(manager.initialize());
    capture::windows::MonitorCaptureService service(manager);
    snip::SnapshotReadback reader(manager);
    snip::SnapshotBatch batch(service, manager, reader.function());
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    std::vector<snip::FrozenMonitor> images;
    bool failed = false, expired = false;
    QObject::connect(&batch, &snip::SnapshotBatch::ready, &loop, [&](auto result) {
        images = std::move(result);
        loop.quit();
    });
    QObject::connect(&batch, &snip::SnapshotBatch::failed, &loop, [&](auto) {
        failed = true;
        loop.quit();
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
        expired = true;
        batch.cancel();
        loop.quit();
    });
    const auto started = std::chrono::steady_clock::now();
    deadline.start(static_cast<int>(catalog.monitors().size()) * 2500 + 3000);
    batch.start(catalog.monitors());
    if (images.empty() && !failed)
        loop.exec();
    deadline.stop();
    batch.cancel();
    REQUIRE_FALSE(expired);
    REQUIRE_FALSE(failed);
    REQUIRE(images.size() == catalog.monitors().size());
    for (std::size_t i = 0; i < images.size(); ++i) {
        CHECK(images[i].image.size() == images[i].geometry.size());
        CHECK(images[i].image.devicePixelRatio() == 1.0);
        const auto crop =
            snip::composeSelection(images, QRect(images[i].geometry.topLeft(), QSize(1, 1)));
        REQUIRE(crop.size() == QSize(1, 1));
    }
    std::cout << "snapshot-batch-monitors=" << images.size() << " readback-elapsed-ms="
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - started)
                     .count()
              << '\n';
}

TEST_CASE("every overlay keeps the exact native monitor rectangle after DPI initialization") {
    platform::windows::WindowsDisplayTopologySource source;
    platform::windows::DisplayCatalog catalog(source);
    REQUIRE(std::holds_alternative<std::vector<platform::windows::MonitorDescriptor>>(
        catalog.refresh()));

    for (const auto& monitor : catalog.monitors()) {
        const auto physicalWidth = static_cast<int>(platform::width(monitor.desktopRect));
        const auto physicalHeight = static_cast<int>(platform::height(monitor.desktopRect));
        QImage image(physicalWidth, physicalHeight, QImage::Format_RGB32);
        REQUIRE_FALSE(image.isNull());
        image.fill(Qt::black);
        snip::SelectionModel selection;
        const QRect geometry(monitor.desktopRect.left, monitor.desktopRect.top, physicalWidth,
                             physicalHeight);
        selection.setBounds(geometry);
        snip::SnipOverlay overlay({geometry, std::move(image), monitor.nativeHandle}, selection);
        bool invalidated{};
        QObject::connect(&overlay, &snip::SnipOverlay::displayInvalidated,
                         [&invalidated] { invalidated = true; });

        overlay.show();
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();

        RECT actual{};
        REQUIRE(GetWindowRect(reinterpret_cast<HWND>(overlay.winId()), &actual) != FALSE);
        CHECK(actual.left == monitor.desktopRect.left);
        CHECK(actual.top == monitor.desktopRect.top);
        CHECK(actual.right == monitor.desktopRect.right);
        CHECK(actual.bottom == monitor.desktopRect.bottom);
        CHECK_FALSE(invalidated);
        overlay.hide();
    }
}

int main(int argc, char* argv[]) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (platform::windows::ensurePerMonitorV2().status ==
        platform::windows::DpiSetupStatus::Failed) {
        return 1;
    }
    QApplication application(argc, argv);
    return Catch::Session().run(argc, argv);
}
