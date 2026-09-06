#pragma once

#include <d3d11.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace lc::capture::windows {

// HRESULT failures remain hresult_error here; the service translates them before
// returning to either a WinRT delegate or a Qt event boundary.
winrt::Windows::Graphics::Capture::GraphicsCaptureItem createItemForMonitor(HMONITOR monitor);

winrt::com_ptr<ID3D11Texture2D> textureFromSurface(
    const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& surface);

}  // namespace lc::capture::windows
