#include "graphics/d3d11/D3d11DeviceFactory.hpp"
#include "graphics/d3d11/D3d11DeviceManager.hpp"
#include "graphics/d3d11/TextureCopy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <utility>
#include <variant>

namespace {

using lc::graphics::d3d11::D3d11DeviceFactory;
using lc::graphics::d3d11::D3d11DeviceManager;
using lc::graphics::d3d11::D3dCreateResult;
using lc::graphics::d3d11::D3dDeviceBundle;
using lc::graphics::d3d11::D3dDriverKind;
using lc::graphics::d3d11::D3dError;
using lc::graphics::d3d11::ID3d11DeviceFactory;
using lc::graphics::d3d11::TextureCopy;
using lc::graphics::d3d11::TextureReadback;

class WarpFactory final : public ID3d11DeviceFactory {
public:
    explicit WarpFactory(D3dCreateResult result) : result_(std::move(result)) {}

    D3dCreateResult create(D3dDriverKind) override {
        return std::move(result_);
    }

private:
    D3dCreateResult result_;
};

struct WarpFixture final {
    WarpFixture() {
        D3d11DeviceFactory realFactory;
        auto result = realFactory.create(D3dDriverKind::Warp);
        REQUIRE(std::holds_alternative<D3dDeviceBundle>(result));
        auto factory = std::make_unique<WarpFactory>(std::move(result));
        manager = std::make_unique<D3d11DeviceManager>(std::move(factory));
        REQUIRE(manager->initialize());
        REQUIRE(manager->current()->driverKind == D3dDriverKind::Warp);
    }

    winrt::com_ptr<ID3D11Texture2D> createTexture(
        DXGI_FORMAT format,
        UINT rowPitch,
        const void* pixels) const {
        D3D11_TEXTURE2D_DESC description{};
        description.Width = 2;
        description.Height = 2;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = format;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        const D3D11_SUBRESOURCE_DATA initial{pixels, rowPitch, 0};
        winrt::com_ptr<ID3D11Texture2D> texture;
        const auto current = manager->current();
        REQUIRE(current.has_value());
        REQUIRE(SUCCEEDED(current->device->CreateTexture2D(
            &description, &initial, texture.put())));
        return texture;
    }

    std::unique_ptr<D3d11DeviceManager> manager;
};

void checkRows(
    const TextureReadback& readback,
    const std::byte* expected,
    std::size_t packedRowBytes) {
    REQUIRE(readback.rowPitch >= packedRowBytes);
    REQUIRE(readback.bytes.size() >= readback.rowPitch * 2);
    CHECK(std::memcmp(readback.bytes.data(), expected, packedRowBytes) == 0);
    CHECK(std::memcmp(
              readback.bytes.data() + readback.rowPitch,
              expected + packedRowBytes,
              packedRowBytes) == 0);
}

}  // namespace

TEST_CASE("copyOwned preserves a BGRA8 texture after the source is released") {
    WarpFixture fixture;
    const std::array<std::byte, 16> pixels{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
        std::byte{0x50}, std::byte{0x60}, std::byte{0x70}, std::byte{0x80},
        std::byte{0x90}, std::byte{0xa0}, std::byte{0xb0}, std::byte{0xc0},
        std::byte{0xd0}, std::byte{0xe0}, std::byte{0xf0}, std::byte{0x00},
    };
    auto source = fixture.createTexture(DXGI_FORMAT_B8G8R8A8_UNORM, 8, pixels.data());
    TextureCopy copier(*fixture.manager);

    auto copyResult = copier.copyOwned(*source);
    REQUIRE(std::holds_alternative<winrt::com_ptr<ID3D11Texture2D>>(copyResult));
    auto owned = std::get<winrt::com_ptr<ID3D11Texture2D>>(std::move(copyResult));
    source = nullptr;

    D3D11_TEXTURE2D_DESC description{};
    owned->GetDesc(&description);
    CHECK(description.Width == 2);
    CHECK(description.Height == 2);
    CHECK(description.Format == DXGI_FORMAT_B8G8R8A8_UNORM);
    CHECK(description.Usage == D3D11_USAGE_DEFAULT);
    CHECK(description.CPUAccessFlags == 0);
    CHECK(description.MiscFlags == 0);

    auto readbackResult = copier.readback(*owned);
    REQUIRE(std::holds_alternative<TextureReadback>(readbackResult));
    const auto& readback = std::get<TextureReadback>(readbackResult);
    CHECK(readback.size.width == 2);
    CHECK(readback.size.height == 2);
    CHECK(readback.format == DXGI_FORMAT_B8G8R8A8_UNORM);
    std::cout << "WARP readback metadata: size=" << readback.size.width << 'x'
              << readback.size.height
              << " format=" << static_cast<unsigned>(readback.format)
              << " rowPitch=" << readback.rowPitch << '\n';
    checkRows(readback, pixels.data(), 8);
}

TEST_CASE("copyOwned preserves an RGBA16F texture description and rows") {
    WarpFixture fixture;
    const std::array<std::uint16_t, 16> pixels{
        0x3c00, 0x3800, 0x0000, 0x3c00,
        0x4000, 0x4200, 0x4400, 0x3c00,
        0x4500, 0x4600, 0x4700, 0x3c00,
        0x4800, 0x4900, 0x4a00, 0x3c00,
    };
    auto source = fixture.createTexture(
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        16,
        pixels.data());
    TextureCopy copier(*fixture.manager);

    auto copyResult = copier.copyOwned(*source);
    REQUIRE(std::holds_alternative<winrt::com_ptr<ID3D11Texture2D>>(copyResult));
    auto owned = std::get<winrt::com_ptr<ID3D11Texture2D>>(std::move(copyResult));
    source = nullptr;

    D3D11_TEXTURE2D_DESC description{};
    owned->GetDesc(&description);
    CHECK(description.Width == 2);
    CHECK(description.Height == 2);
    CHECK(description.Format == DXGI_FORMAT_R16G16B16A16_FLOAT);
    CHECK(description.Usage == D3D11_USAGE_DEFAULT);
    CHECK(description.CPUAccessFlags == 0);
    CHECK(description.MiscFlags == 0);

    auto readbackResult = copier.readback(*owned);
    REQUIRE(std::holds_alternative<TextureReadback>(readbackResult));
    const auto& readback = std::get<TextureReadback>(readbackResult);
    CHECK(readback.size.width == 2);
    CHECK(readback.size.height == 2);
    CHECK(readback.format == DXGI_FORMAT_R16G16B16A16_FLOAT);
    std::cout << "WARP readback metadata: size=" << readback.size.width << 'x'
              << readback.size.height
              << " format=" << static_cast<unsigned>(readback.format)
              << " rowPitch=" << readback.rowPitch << '\n';
    checkRows(
        readback,
        reinterpret_cast<const std::byte*>(pixels.data()),
        16);
}

TEST_CASE("texture operations reject a different device and an uninitialized manager") {
    WarpFixture fixture;
    WarpFixture other;
    const std::array<std::byte, 16> pixels{};
    auto source = fixture.createTexture(DXGI_FORMAT_B8G8R8A8_UNORM, 8, pixels.data());
    TextureCopy foreign(*other.manager);
    const auto copied = foreign.copyOwned(*source);
    REQUIRE(std::holds_alternative<D3dError>(copied));
    CHECK(std::get<D3dError>(copied).code ==
          lc::graphics::d3d11::D3dErrorCode::InvalidTexture);
    const auto read = foreign.readback(*source);
    REQUIRE(std::holds_alternative<D3dError>(read));
    CHECK(std::get<D3dError>(read).code ==
          lc::graphics::d3d11::D3dErrorCode::InvalidTexture);

    D3d11DeviceManager uninitialized;
    TextureCopy unavailable(uninitialized);
    const auto missingCopy = unavailable.copyOwned(*source);
    REQUIRE(std::holds_alternative<D3dError>(missingCopy));
    CHECK(std::get<D3dError>(missingCopy).code ==
          lc::graphics::d3d11::D3dErrorCode::DeviceLost);
    const auto missingRead = unavailable.readback(*source);
    REQUIRE(std::holds_alternative<D3dError>(missingRead));
    CHECK(std::get<D3dError>(missingRead).code ==
          lc::graphics::d3d11::D3dErrorCode::DeviceLost);
}
