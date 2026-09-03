#include "platform/windows/DisplayCatalog.hpp"
#include "platform/windows/WindowsDisplayTopologySource.hpp"

#include <Windows.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using lc::platform::PhysicalPoint;
using lc::platform::PhysicalRect;
using lc::platform::windows::DisplayError;
using lc::platform::windows::DisplayErrorCode;
using lc::platform::windows::DisplayPathRecord;
using lc::platform::windows::IDisplayTopologySource;
using lc::platform::windows::NativeMonitorRecord;
using lc::platform::windows::TopologyReadResult;
using lc::platform::windows::TopologySnapshot;

HMONITOR fakeHandle(std::uintptr_t value) {
    return reinterpret_cast<HMONITOR>(value);
}

LUID luid(std::uint32_t lowPart, std::int32_t highPart) {
    return LUID{lowPart, highPart};
}

NativeMonitorRecord monitor(HMONITOR handle,
                            std::wstring deviceName,
                            PhysicalRect desktopRect,
                            bool primary = false) {
    return NativeMonitorRecord{
        handle,
        std::move(deviceName),
        desktopRect,
        desktopRect,
        144,
        144,
        primary,
    };
}

DisplayPathRecord path(std::wstring deviceName,
                       std::wstring friendlyName,
                       std::wstring devicePath,
                       LUID adapterId,
                       std::uint32_t targetId,
                       bool hdrEnabled = false) {
    return DisplayPathRecord{
        std::move(deviceName),
        std::move(friendlyName),
        std::move(devicePath),
        adapterId,
        targetId,
        hdrEnabled,
    };
}

TopologySnapshot twoMonitorSnapshot() {
    TopologySnapshot snapshot;
    snapshot.monitors = {
        monitor(fakeHandle(1), L"\\\\.\\DISPLAY1", {-1920, 0, 0, 1080}),
        monitor(fakeHandle(2), L"\\\\.\\DISPLAY10", {0, 0, 1920, 1080}, true),
    };
    snapshot.paths = {
        path(L"\\\\.\\DISPLAY10",
             L"Right display",
             L"\\\\?\\DISPLAY#RIGHT",
             luid(0x22222222U, 0x11111111),
             10,
             true),
        path(L"\\\\.\\DISPLAY1",
             L"Left display",
             L"\\\\?\\DISPLAY#LEFT",
             luid(0x44444444U, 0x33333333),
             1),
    };
    return snapshot;
}

class FakeTopologySource final : public IDisplayTopologySource {
public:
    explicit FakeTopologySource(std::vector<TopologyReadResult> results)
        : results_(std::move(results)) {}

    TopologyReadResult read() override {
        REQUIRE(next_ < results_.size());
        return results_[next_++];
    }

private:
    std::vector<TopologyReadResult> results_;
    std::size_t next_{0};
};

}  // namespace

TEST_CASE("display refresh joins monitors by exact GDI device name") {
    FakeTopologySource source{{twoMonitorSnapshot()}};
    lc::platform::windows::DisplayCatalog catalog{source};

    const auto result = catalog.refresh();

    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(result));
    const auto& displays =
        std::get<std::vector<lc::platform::windows::MonitorDescriptor>>(result);
    REQUIRE(displays.size() == 2);
    CHECK(displays[0].gdiDeviceName == L"\\\\.\\DISPLAY1");
    CHECK(displays[0].displayName == L"Left display");
    CHECK(displays[0].targetId == 1);
    CHECK(displays[0].adapterId.LowPart == 0x44444444U);
    CHECK(displays[1].gdiDeviceName == L"\\\\.\\DISPLAY10");
    CHECK(displays[1].displayName == L"Right display");
    CHECK(displays[1].targetId == 10);
    CHECK(displays[1].hdrEnabled);
}

TEST_CASE("display refresh rejects duplicate or missing path mappings") {
    SECTION("duplicate mapping") {
        auto snapshot = twoMonitorSnapshot();
        snapshot.paths.push_back(path(L"\\\\.\\DISPLAY1",
                                      L"Duplicate",
                                      L"\\\\?\\DISPLAY#DUPLICATE",
                                      luid(0x66666666U, 0x55555555),
                                      3));
        FakeTopologySource source{{std::move(snapshot)}};
        lc::platform::windows::DisplayCatalog catalog{source};

        const auto result = catalog.refresh();

        REQUIRE(std::holds_alternative<DisplayError>(result));
        const auto error = std::get<DisplayError>(result);
        CHECK(error.code == DisplayErrorCode::TopologyValidationFailed);
        CHECK(error.nativeError == ERROR_INVALID_DATA);
        CHECK(catalog.generation() == 0);
        CHECK_FALSE(catalog.healthy());
    }

    SECTION("missing mapping") {
        auto snapshot = twoMonitorSnapshot();
        snapshot.paths.pop_back();
        FakeTopologySource source{{std::move(snapshot)}};
        lc::platform::windows::DisplayCatalog catalog{source};

        const auto result = catalog.refresh();

        REQUIRE(std::holds_alternative<DisplayError>(result));
        const auto error = std::get<DisplayError>(result);
        CHECK(error.code == DisplayErrorCode::TopologyValidationFailed);
        CHECK(error.nativeError == ERROR_NOT_FOUND);
        CHECK(catalog.generation() == 0);
        CHECK_FALSE(catalog.healthy());
    }
}

TEST_CASE("failed refresh keeps the previous snapshot but marks it unhealthy") {
    const DisplayError readFailure{DisplayErrorCode::DpiQueryFailed, ERROR_GEN_FAILURE};
    FakeTopologySource source{{twoMonitorSnapshot(), readFailure}};
    lc::platform::windows::DisplayCatalog catalog{source};
    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(
        catalog.refresh()));

    const auto result = catalog.refresh();

    REQUIRE(std::holds_alternative<DisplayError>(result));
    CHECK(std::get<DisplayError>(result).code == DisplayErrorCode::DpiQueryFailed);
    CHECK(catalog.generation() == 1);
    CHECK_FALSE(catalog.healthy());
    const auto retained = catalog.findByNativeHandle(fakeHandle(1));
    REQUIRE(retained.has_value());
    CHECK(retained->desktopRect.left == -1920);
    CHECK(retained->catalogGeneration == 1);
}

TEST_CASE("successful refresh increments generation and stamps every monitor") {
    FakeTopologySource source{{twoMonitorSnapshot(), twoMonitorSnapshot()}};
    lc::platform::windows::DisplayCatalog catalog{source};

    const auto first = catalog.refresh();
    const auto second = catalog.refresh();

    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(first));
    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(second));
    CHECK(catalog.generation() == 2);
    CHECK(catalog.healthy());
    for (const auto& display :
         std::get<std::vector<lc::platform::windows::MonitorDescriptor>>(first)) {
        CHECK(display.catalogGeneration == 1);
    }
    for (const auto& display :
         std::get<std::vector<lc::platform::windows::MonitorDescriptor>>(second)) {
        CHECK(display.catalogGeneration == 2);
    }
}

TEST_CASE("monitor lookup handles negative coordinates and half-open edges") {
    FakeTopologySource source{{twoMonitorSnapshot()}};
    lc::platform::windows::DisplayCatalog catalog{source};
    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(
        catalog.refresh()));

    const auto negativeInterior = catalog.monitorContaining(PhysicalPoint{-1, 100});
    const auto negativeOuterEdge = catalog.monitorContaining(PhysicalPoint{-1920, 0});
    const auto sharedEdge = catalog.monitorContaining(PhysicalPoint{0, 100});

    REQUIRE(negativeInterior.has_value());
    REQUIRE(negativeOuterEdge.has_value());
    REQUIRE(sharedEdge.has_value());
    CHECK(negativeInterior->nativeHandle == fakeHandle(1));
    CHECK(negativeOuterEdge->nativeHandle == fakeHandle(1));
    CHECK(sharedEdge->nativeHandle == fakeHandle(2));
    CHECK_FALSE(catalog.monitorContaining(PhysicalPoint{1920, 100}).has_value());
    CHECK_FALSE(catalog.monitorContaining(PhysicalPoint{100, 1080}).has_value());
}

TEST_CASE("monitor lookup from a Win32 point reuses the catalog native handle") {
    const POINT point{0, 0};
    const HMONITOR nativeHandle = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    REQUIRE(nativeHandle != nullptr);
    TopologySnapshot snapshot;
    snapshot.monitors = {
        monitor(nativeHandle, L"\\\\.\\DISPLAY1", {0, 0, 1920, 1080}, true),
    };
    snapshot.paths = {
        path(L"\\\\.\\DISPLAY1",
             L"Native point display",
             L"\\\\?\\DISPLAY#NATIVE",
             luid(0x22222222U, 0x11111111),
             1),
    };
    FakeTopologySource source{{std::move(snapshot)}};
    lc::platform::windows::DisplayCatalog catalog{source};
    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(
        catalog.refresh()));

    const auto found = catalog.monitorFromPoint(point);

    REQUIRE(found.has_value());
    CHECK(found->nativeHandle == nativeHandle);
}

TEST_CASE("monitor ids are deterministic UTF-8 values") {
    auto snapshot = twoMonitorSnapshot();
    snapshot.paths[1] = path(L"\\\\.\\DISPLAY1",
                             L"Unicode display",
                             L"\\\\?\\DISPLAY#\u6A21\u578B#\u663E\u793A\u5668",
                             luid(0x9ABCDEF0U, 0x12345678),
                             7);
    FakeTopologySource source{{snapshot, snapshot}};
    lc::platform::windows::DisplayCatalog catalog{source};

    const auto first = catalog.refresh();
    const auto second = catalog.refresh();

    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(first));
    REQUIRE(std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(second));
    const std::string expected =
        "123456789abcdef0:7:\\\\?\\DISPLAY#"
        "\xE6\xA8\xA1\xE5\x9E\x8B#\xE6\x98\xBE\xE7\xA4\xBA\xE5\x99\xA8";
    CHECK(std::get<std::vector<lc::platform::windows::MonitorDescriptor>>(first)[0].id.value ==
          expected);
    CHECK(std::get<std::vector<lc::platform::windows::MonitorDescriptor>>(second)[0].id.value ==
          expected);
}

TEST_CASE("Windows display topology source enumerates safe monitor diagnostics") {
    lc::platform::windows::WindowsDisplayTopologySource source;

    const auto result = source.read();

    REQUIRE(std::holds_alternative<TopologySnapshot>(result));
    const auto& snapshot = std::get<TopologySnapshot>(result);
    REQUIRE_FALSE(snapshot.monitors.empty());
    std::cout << "Active monitor count: " << snapshot.monitors.size() << '\n';
    for (std::size_t index = 0; index < snapshot.monitors.size(); ++index) {
        const auto& nativeMonitor = snapshot.monitors[index];
        const auto matchingPath = std::find_if(
            snapshot.paths.begin(), snapshot.paths.end(), [&nativeMonitor](const auto& candidate) {
                return candidate.gdiDeviceName == nativeMonitor.gdiDeviceName;
            });
        REQUIRE(matchingPath != snapshot.paths.end());
        std::cout << "Monitor " << index << ": rect=[" << nativeMonitor.desktopRect.left << ','
                  << nativeMonitor.desktopRect.top << ',' << nativeMonitor.desktopRect.right << ','
                  << nativeMonitor.desktopRect.bottom << "] dpi=" << nativeMonitor.dpiX << 'x'
                  << nativeMonitor.dpiY << " hdr="
                  << (matchingPath->hdrEnabled ? "true" : "false") << '\n';
    }
}
