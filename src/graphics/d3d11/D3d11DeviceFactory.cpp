#include "graphics/d3d11/D3d11DeviceFactory.hpp"

#include <dxgi.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <array>

namespace lc::graphics::d3d11 {
namespace {

constexpr bool defaultDebugLayerRequested() noexcept {
#ifdef _DEBUG
    return true;
#else
    return false;
#endif
}

D3D_DRIVER_TYPE nativeDriverType(D3dDriverKind kind) noexcept {
    return kind == D3dDriverKind::Hardware ? D3D_DRIVER_TYPE_HARDWARE
                                           : D3D_DRIVER_TYPE_WARP;
}

}  // namespace

D3d11DeviceFactory::D3d11DeviceFactory()
    : D3d11DeviceFactory(&D3D11CreateDevice, defaultDebugLayerRequested()) {}

D3d11DeviceFactory::D3d11DeviceFactory(
    D3d11CreateDeviceFunction createDevice,
    bool requestDebugLayer)
    : createDevice_(createDevice), requestDebugLayer_(requestDebugLayer) {}

D3dCreateResult D3d11DeviceFactory::create(D3dDriverKind kind) {
    constexpr std::array featureLevels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (requestDebugLayer_) {
        flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL selectedFeatureLevel{};
    auto create = [&](UINT creationFlags) {
        device = nullptr;
        context = nullptr;
        selectedFeatureLevel = {};
        return createDevice_(
            nullptr,
            nativeDriverType(kind),
            nullptr,
            creationFlags,
            featureLevels.data(),
            static_cast<UINT>(featureLevels.size()),
            D3D11_SDK_VERSION,
            device.put(),
            &selectedFeatureLevel,
            context.put());
    };

    HRESULT result = create(flags);
    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING &&
        (flags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = create(flags);
    }
    if (FAILED(result)) {
        return D3dError{D3dErrorCode::CreationFailed, result};
    }

    try {
        const auto dxgiDevice = device.as<IDXGIDevice>();
        winrt::com_ptr<IInspectable> inspectable;
        result = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put());
        if (FAILED(result)) {
            return D3dError{D3dErrorCode::CreationFailed, result};
        }

        D3dDeviceBundle bundle;
        bundle.device = std::move(device);
        bundle.immediateContext = std::move(context);
        bundle.winrtDevice = inspectable.as<
            winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        bundle.featureLevel = selectedFeatureLevel;
        bundle.driverKind = kind;
        return bundle;
    } catch (const winrt::hresult_error& error) {
        return D3dError{D3dErrorCode::CreationFailed, error.code()};
    }
}

}  // namespace lc::graphics::d3d11
