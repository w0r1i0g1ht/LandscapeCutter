#pragma once

#include "graphics/d3d11/D3d11DeviceFactory.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace lc::graphics::d3d11 {

class ID3d11DeviceRecovery {
public:
    virtual ~ID3d11DeviceRecovery() = default;
    virtual bool rebuild() = 0;
    virtual std::uint64_t generation() const noexcept = 0;
};

using ImmediateContextOperation =
    std::function<std::optional<D3dError>(ID3D11Device&, ID3D11DeviceContext&)>;

class D3d11DeviceManager final : public ID3d11DeviceRecovery {
public:
    D3d11DeviceManager();
    explicit D3d11DeviceManager(std::unique_ptr<ID3d11DeviceFactory> factory);

    bool initialize();
    bool rebuild() override;
    std::uint64_t generation() const noexcept override;

    std::optional<D3dDeviceBundle> current() const;
    std::optional<D3dError> lastError() const;
    std::optional<D3dError> withImmediateContext(
        const ImmediateContextOperation& operation);

private:
    bool createAndPublish();

    std::unique_ptr<ID3d11DeviceFactory> factory_;
    mutable std::mutex mutex_;
    std::optional<D3dDeviceBundle> current_;
    std::optional<D3dError> lastError_;
    std::uint64_t generation_{};
};

}  // namespace lc::graphics::d3d11
