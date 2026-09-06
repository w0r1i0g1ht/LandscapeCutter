#include "capture/windows/GraphicsCaptureInterop.hpp"

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

namespace lc::capture::windows {

winrt::Windows::Graphics::Capture::GraphicsCaptureItem createItemForMonitor(HMONITOR monitor) {
    if (!monitor) { winrt::throw_hresult(E_INVALIDARG); }
    using winrt::Windows::Graphics::Capture::GraphicsCaptureItem;
    const auto interop = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    GraphicsCaptureItem item{nullptr};
    winrt::check_hresult(interop->CreateForMonitor(
        monitor, winrt::guid_of<GraphicsCaptureItem>(), winrt::put_abi(item)));
    return item;
}

winrt::com_ptr<ID3D11Texture2D> textureFromSurface(
    const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& surface) {
    if (!surface) { winrt::throw_hresult(E_INVALIDARG); }
    const auto access = surface.as<
        ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
    return texture;
}

}  // namespace lc::capture::windows
