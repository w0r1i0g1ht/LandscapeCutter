#pragma once

#include "graphics/d3d11/D3d11DeviceManager.hpp"
#include "platform/MonitorTypes.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace lc::graphics::d3d11 {

struct TextureReadback final {
    platform::PixelSize size;
    DXGI_FORMAT format;
    std::size_t rowPitch;
    std::vector<std::byte> bytes;
};

class TextureCopy final {
public:
    explicit TextureCopy(D3d11DeviceManager& manager);

    std::variant<winrt::com_ptr<ID3D11Texture2D>, D3dError> copyOwned(
        ID3D11Texture2D& source);
    std::variant<TextureReadback, D3dError> readback(ID3D11Texture2D& source);

private:
    D3d11DeviceManager& manager_;
};

}  // namespace lc::graphics::d3d11
