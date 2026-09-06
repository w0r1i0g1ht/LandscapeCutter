#include "capture/windows/GraphicsCaptureInterop.hpp"

#include <QCoreApplication>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <d3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>

TEST_CASE("interop extracts the same D3D texture from a synthetic WinRT surface") {
    winrt::com_ptr<ID3D11Device> device;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        device.put(), nullptr, nullptr)));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 2;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    winrt::com_ptr<ID3D11Texture2D> texture;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, texture.put())));
    winrt::com_ptr<IInspectable> inspectable;
    REQUIRE(SUCCEEDED(CreateDirect3D11SurfaceFromDXGISurface(
        texture.as<IDXGISurface>().get(), inspectable.put())));
    auto surface = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>();
    auto extracted = lc::capture::windows::textureFromSurface(surface);
    CHECK(extracted.get() == texture.get());
}

TEST_CASE("interop invalid monitor and null surface expose HRESULT errors") {
    CHECK_THROWS_AS(lc::capture::windows::createItemForMonitor(nullptr), winrt::hresult_error);
    CHECK_THROWS_AS(lc::capture::windows::textureFromSurface(nullptr), winrt::hresult_error);
}

int main(int argc, char* argv[]) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    QCoreApplication application(argc, argv);
    const int result = Catch::Session{}.run(argc, argv);
    winrt::uninit_apartment();
    return result;
}
