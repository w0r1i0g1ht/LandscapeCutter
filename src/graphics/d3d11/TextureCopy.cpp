#include "graphics/d3d11/TextureCopy.hpp"

#include <wil/resource.h>

#include <cstring>
#include <limits>

namespace lc::graphics::d3d11 {
namespace {

bool belongsTo(ID3D11Texture2D& texture, ID3D11Device& device) {
    winrt::com_ptr<ID3D11Device> owner;
    texture.GetDevice(owner.put());
    return owner.get() == &device;
}

}  // namespace

TextureCopy::TextureCopy(D3d11DeviceManager& manager) : manager_(manager) {}

std::variant<winrt::com_ptr<ID3D11Texture2D>, D3dError> TextureCopy::copyOwned(
    ID3D11Texture2D& source) {
    std::variant<winrt::com_ptr<ID3D11Texture2D>, D3dError> result{
        D3dError{D3dErrorCode::CopyFailed, E_FAIL}};
    const auto operationError = manager_.withImmediateContext(
        [&](ID3D11Device& device,
            ID3D11DeviceContext& context) -> std::optional<D3dError> {
            if (!belongsTo(source, device)) {
                result = D3dError{D3dErrorCode::InvalidTexture, E_INVALIDARG};
                return std::get<D3dError>(result);
            }

            D3D11_TEXTURE2D_DESC description{};
            source.GetDesc(&description);
            if (description.Width == 0 || description.Height == 0) {
                result = D3dError{D3dErrorCode::InvalidTexture, E_INVALIDARG};
                return std::get<D3dError>(result);
            }
            description.Usage = D3D11_USAGE_DEFAULT;
            description.CPUAccessFlags = 0;
            description.MiscFlags = 0;

            winrt::com_ptr<ID3D11Texture2D> owned;
            const HRESULT created =
                device.CreateTexture2D(&description, nullptr, owned.put());
            if (FAILED(created)) {
                result = D3dError{D3dErrorCode::CopyFailed, created};
                return std::get<D3dError>(result);
            }

            context.CopyResource(owned.get(), &source);
            const HRESULT deviceState = device.GetDeviceRemovedReason();
            if (FAILED(deviceState)) {
                result = D3dError{D3dErrorCode::DeviceLost, deviceState};
                return std::get<D3dError>(result);
            }
            result = std::move(owned);
            return std::nullopt;
        });
    if (operationError) {
        return *operationError;
    }
    return result;
}

std::variant<TextureReadback, D3dError> TextureCopy::readback(
    ID3D11Texture2D& source) {
    std::variant<TextureReadback, D3dError> result{
        D3dError{D3dErrorCode::ReadbackFailed, E_FAIL}};
    const auto operationError = manager_.withImmediateContext(
        [&](ID3D11Device& device,
            ID3D11DeviceContext& context) -> std::optional<D3dError> {
            if (!belongsTo(source, device)) {
                result = D3dError{D3dErrorCode::InvalidTexture, E_INVALIDARG};
                return std::get<D3dError>(result);
            }

            D3D11_TEXTURE2D_DESC description{};
            source.GetDesc(&description);
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            description.MiscFlags = 0;

            winrt::com_ptr<ID3D11Texture2D> staging;
            HRESULT nativeCode =
                device.CreateTexture2D(&description, nullptr, staging.put());
            if (FAILED(nativeCode)) {
                result = D3dError{D3dErrorCode::ReadbackFailed, nativeCode};
                return std::get<D3dError>(result);
            }

            context.CopyResource(staging.get(), &source);
            nativeCode = device.GetDeviceRemovedReason();
            if (FAILED(nativeCode)) {
                result = D3dError{D3dErrorCode::DeviceLost, nativeCode};
                return std::get<D3dError>(result);
            }

            D3D11_MAPPED_SUBRESOURCE mapped{};
            nativeCode = context.Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (FAILED(nativeCode)) {
                result = D3dError{D3dErrorCode::ReadbackFailed, nativeCode};
                return std::get<D3dError>(result);
            }

            const auto unmap = wil::scope_exit(
                [&] { context.Unmap(staging.get(), 0); });
            if (mapped.RowPitch >
                std::numeric_limits<std::size_t>::max() / description.Height) {
                result = D3dError{D3dErrorCode::ReadbackFailed, E_OUTOFMEMORY};
                return std::get<D3dError>(result);
            }

            TextureReadback readback{
                {description.Width, description.Height},
                description.Format,
                mapped.RowPitch,
                std::vector<std::byte>(
                    static_cast<std::size_t>(mapped.RowPitch) * description.Height),
            };
            for (UINT row = 0; row < description.Height; ++row) {
                const auto* sourceRow =
                    static_cast<const std::byte*>(mapped.pData) +
                    static_cast<std::size_t>(mapped.RowPitch) * row;
                std::memcpy(
                    readback.bytes.data() + readback.rowPitch * row,
                    sourceRow,
                    readback.rowPitch);
            }
            result = std::move(readback);
            return std::nullopt;
        });
    if (operationError) {
        return *operationError;
    }
    return result;
}

}  // namespace lc::graphics::d3d11
