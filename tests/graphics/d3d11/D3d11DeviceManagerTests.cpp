#include "graphics/d3d11/D3d11DeviceFactory.hpp"
#include "graphics/d3d11/D3d11DeviceManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {

using lc::graphics::d3d11::D3d11DeviceFactory;
using lc::graphics::d3d11::D3d11DeviceManager;
using lc::graphics::d3d11::D3dCreateResult;
using lc::graphics::d3d11::D3dDeviceBundle;
using lc::graphics::d3d11::D3dDriverKind;
using lc::graphics::d3d11::D3dError;
using lc::graphics::d3d11::D3dErrorCode;
using lc::graphics::d3d11::ID3d11DeviceFactory;

D3dDeviceBundle bundle(D3dDriverKind kind) {
    D3dDeviceBundle value;
    value.featureLevel = D3D_FEATURE_LEVEL_11_0;
    value.driverKind = kind;
    return value;
}

D3dError creationFailure(HRESULT nativeCode = E_FAIL) {
    return {D3dErrorCode::CreationFailed, nativeCode};
}

class ScriptedFactory final : public ID3d11DeviceFactory {
public:
    explicit ScriptedFactory(std::vector<D3dCreateResult> results)
        : results_(std::move(results)) {}

    D3dCreateResult create(D3dDriverKind kind) override {
        calls.push_back(kind);
        REQUIRE(next_ < results_.size());
        return std::move(results_[next_++]);
    }

    std::vector<D3dDriverKind> calls;

private:
    std::vector<D3dCreateResult> results_;
    std::size_t next_{};
};

struct DebugFallbackProbe final {
    static inline std::vector<UINT> flags;

    static HRESULT WINAPI create(
        IDXGIAdapter* adapter,
        D3D_DRIVER_TYPE driverType,
        HMODULE software,
        UINT creationFlags,
        const D3D_FEATURE_LEVEL* featureLevels,
        UINT featureLevelCount,
        UINT sdkVersion,
        ID3D11Device** device,
        D3D_FEATURE_LEVEL* selectedFeatureLevel,
        ID3D11DeviceContext** immediateContext) {
        flags.push_back(creationFlags);
        if ((creationFlags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
            return DXGI_ERROR_SDK_COMPONENT_MISSING;
        }
        return D3D11CreateDevice(
            adapter,
            driverType,
            software,
            creationFlags,
            featureLevels,
            featureLevelCount,
            sdkVersion,
            device,
            selectedFeatureLevel,
            immediateContext);
    }
};

}  // namespace

TEST_CASE("device manager prefers a hardware device") {
    auto factory = std::make_unique<ScriptedFactory>(
        std::vector<D3dCreateResult>{bundle(D3dDriverKind::Hardware)});
    auto* observedFactory = factory.get();
    D3d11DeviceManager manager(std::move(factory));

    REQUIRE(manager.initialize());

    REQUIRE(observedFactory->calls == std::vector{D3dDriverKind::Hardware});
    const auto current = manager.current();
    REQUIRE(current.has_value());
    CHECK(current->driverKind == D3dDriverKind::Hardware);
    CHECK(manager.generation() == 1);
    CHECK_FALSE(manager.lastError().has_value());
}

TEST_CASE("device manager falls back to WARP after hardware failure") {
    auto factory = std::make_unique<ScriptedFactory>(std::vector<D3dCreateResult>{
        creationFailure(DXGI_ERROR_UNSUPPORTED), bundle(D3dDriverKind::Warp)});
    auto* observedFactory = factory.get();
    D3d11DeviceManager manager(std::move(factory));

    REQUIRE(manager.initialize());

    REQUIRE(observedFactory->calls ==
            std::vector{D3dDriverKind::Hardware, D3dDriverKind::Warp});
    REQUIRE(manager.current().has_value());
    CHECK(manager.current()->driverKind == D3dDriverKind::Warp);
    CHECK(manager.generation() == 1);
}

TEST_CASE("device manager reports failure when both drivers fail") {
    auto factory = std::make_unique<ScriptedFactory>(std::vector<D3dCreateResult>{
        creationFailure(E_FAIL), creationFailure(DXGI_ERROR_UNSUPPORTED)});
    auto* observedFactory = factory.get();
    D3d11DeviceManager manager(std::move(factory));

    CHECK_FALSE(manager.initialize());

    REQUIRE(observedFactory->calls ==
            std::vector{D3dDriverKind::Hardware, D3dDriverKind::Warp});
    CHECK_FALSE(manager.current().has_value());
    CHECK(manager.generation() == 0);
    REQUIRE(manager.lastError().has_value());
    CHECK(manager.lastError()->code == D3dErrorCode::CreationFailed);
    CHECK(manager.lastError()->nativeCode == DXGI_ERROR_UNSUPPORTED);
}

TEST_CASE("each successful rebuild increments device generation once") {
    auto factory = std::make_unique<ScriptedFactory>(std::vector<D3dCreateResult>{
        bundle(D3dDriverKind::Hardware),
        creationFailure(DXGI_ERROR_UNSUPPORTED),
        bundle(D3dDriverKind::Warp),
        creationFailure(E_FAIL),
        creationFailure(E_OUTOFMEMORY),
    });
    D3d11DeviceManager manager(std::move(factory));
    REQUIRE(manager.initialize());
    REQUIRE(manager.generation() == 1);

    REQUIRE(manager.rebuild());
    CHECK(manager.generation() == 2);
    REQUIRE(manager.current().has_value());
    CHECK(manager.current()->driverKind == D3dDriverKind::Warp);

    CHECK_FALSE(manager.rebuild());
    CHECK(manager.generation() == 2);
    REQUIRE(manager.current().has_value());
    CHECK(manager.current()->driverKind == D3dDriverKind::Warp);
}

TEST_CASE("real factory creates a BGRA-capable 11.x WinRT device") {
    D3d11DeviceFactory factory;
    auto result = factory.create(D3dDriverKind::Hardware);
    if (std::holds_alternative<D3dError>(result)) {
        result = factory.create(D3dDriverKind::Warp);
    }

    REQUIRE(std::holds_alternative<D3dDeviceBundle>(result));
    const auto& created = std::get<D3dDeviceBundle>(result);
    REQUIRE(created.device);
    REQUIRE(created.immediateContext);
    CHECK(created.winrtDevice != nullptr);
    CHECK((created.device->GetCreationFlags() & D3D11_CREATE_DEVICE_BGRA_SUPPORT) != 0);
    CHECK((created.featureLevel == D3D_FEATURE_LEVEL_11_1 ||
           created.featureLevel == D3D_FEATURE_LEVEL_11_0));
}

TEST_CASE("factory retries without debug when the debug layer is unavailable") {
    DebugFallbackProbe::flags.clear();
    D3d11DeviceFactory factory(&DebugFallbackProbe::create, true);

    const auto result = factory.create(D3dDriverKind::Warp);

    REQUIRE(std::holds_alternative<D3dDeviceBundle>(result));
    REQUIRE(DebugFallbackProbe::flags.size() == 2);
    CHECK((DebugFallbackProbe::flags[0] & D3D11_CREATE_DEVICE_DEBUG) != 0);
    CHECK((DebugFallbackProbe::flags[1] & D3D11_CREATE_DEVICE_DEBUG) == 0);
    CHECK((DebugFallbackProbe::flags[0] & D3D11_CREATE_DEVICE_BGRA_SUPPORT) != 0);
    CHECK((DebugFallbackProbe::flags[1] & D3D11_CREATE_DEVICE_BGRA_SUPPORT) != 0);
}

TEST_CASE("manager serializes all immediate-context operations") {
    D3d11DeviceFactory realFactory;
    auto created = realFactory.create(D3dDriverKind::Warp);
    REQUIRE(std::holds_alternative<D3dDeviceBundle>(created));
    auto factory = std::make_unique<ScriptedFactory>(
        std::vector<D3dCreateResult>{std::move(created)});
    D3d11DeviceManager manager(std::move(factory));
    REQUIRE(manager.initialize());

    std::atomic<int> active{};
    std::atomic<int> maximum{};
    std::atomic<int> failures{};
    const auto operation = [&] {
        const auto error = manager.withImmediateContext(
            [&](ID3D11Device&, ID3D11DeviceContext&) -> std::optional<D3dError> {
                const int now = ++active;
                maximum.store(std::max(maximum.load(), now));
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                --active;
                return std::nullopt;
            });
        if (error) { ++failures; }
    };

    std::thread first(operation);
    std::thread second(operation);
    first.join();
    second.join();

    CHECK(maximum.load() == 1);
    CHECK(failures.load() == 0);
}

TEST_CASE("device manager snapshots retain their matching generation across rebuilds") {
    auto factory = std::make_unique<ScriptedFactory>(std::vector<D3dCreateResult>{
        bundle(D3dDriverKind::Hardware), bundle(D3dDriverKind::Warp)});
    D3d11DeviceManager manager(std::move(factory));
    REQUIRE(manager.initialize());
    const auto before = manager.current();
    REQUIRE(before.has_value());
    REQUIRE(manager.rebuild());
    const auto after = manager.current();
    REQUIRE(after.has_value());
    CHECK(before->generation == 1);
    CHECK(before->driverKind == D3dDriverKind::Hardware);
    CHECK(after->generation == 2);
    CHECK(after->driverKind == D3dDriverKind::Warp);
    CHECK(manager.generation() == after->generation);
}

TEST_CASE("real device manager reports driver metadata without frame content") {
    D3d11DeviceManager manager;
    REQUIRE(manager.initialize());
    const auto current = manager.current();
    REQUIRE(current.has_value());
    REQUIRE(current->device);
    REQUIRE(current->immediateContext);
    REQUIRE(current->winrtDevice != nullptr);
    std::cout << "D3D11 metadata: driver="
              << (current->driverKind == D3dDriverKind::Hardware ? "Hardware" : "WARP")
              << " featureLevel=" << static_cast<unsigned>(current->featureLevel)
              << " generation=" << manager.generation()
              << " bgra=" << ((current->device->GetCreationFlags() &
                                D3D11_CREATE_DEVICE_BGRA_SUPPORT) != 0) << '\n';
}
