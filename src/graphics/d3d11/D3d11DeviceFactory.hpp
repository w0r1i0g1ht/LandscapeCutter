#pragma once

#include <d3d11.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/base.h>

#include <cstdint>
#include <variant>

namespace lc::graphics::d3d11 {

enum class D3dDriverKind {
    Hardware,
    Warp,
};

enum class D3dErrorCode {
    CreationFailed,
    DeviceLost,
    InvalidTexture,
    CopyFailed,
    ReadbackFailed,
};

struct D3dError final {
    D3dErrorCode code;
    HRESULT nativeCode;
};

struct D3dDeviceBundle final {
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> immediateContext;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice{nullptr};
    D3D_FEATURE_LEVEL featureLevel{};
    D3dDriverKind driverKind{};
    std::uint64_t generation{};
};

using D3dCreateResult = std::variant<D3dDeviceBundle, D3dError>;

class ID3d11DeviceFactory {
public:
    virtual ~ID3d11DeviceFactory() = default;
    virtual D3dCreateResult create(D3dDriverKind kind) = 0;
};

using D3d11CreateDeviceFunction = decltype(&D3D11CreateDevice);

class D3d11DeviceFactory final : public ID3d11DeviceFactory {
public:
    D3d11DeviceFactory();
    D3d11DeviceFactory(D3d11CreateDeviceFunction createDevice, bool requestDebugLayer);

    D3dCreateResult create(D3dDriverKind kind) override;

private:
    D3d11CreateDeviceFunction createDevice_;
    bool requestDebugLayer_;
};

}  // namespace lc::graphics::d3d11
