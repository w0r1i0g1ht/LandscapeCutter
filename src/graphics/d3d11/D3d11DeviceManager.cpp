#include "graphics/d3d11/D3d11DeviceManager.hpp"

#include <array>
#include <utility>

namespace lc::graphics::d3d11 {

D3d11DeviceManager::D3d11DeviceManager()
    : D3d11DeviceManager(std::make_unique<D3d11DeviceFactory>()) {}

D3d11DeviceManager::D3d11DeviceManager(
    std::unique_ptr<ID3d11DeviceFactory> factory)
    : factory_(std::move(factory)) {}

bool D3d11DeviceManager::initialize() {
    return createAndPublish();
}

bool D3d11DeviceManager::rebuild() {
    return createAndPublish();
}

std::uint64_t D3d11DeviceManager::generation() const noexcept {
    std::scoped_lock lock(mutex_);
    return generation_;
}

std::optional<D3dDeviceBundle> D3d11DeviceManager::current() const {
    std::scoped_lock lock(mutex_);
    return current_;
}

std::optional<D3dError> D3d11DeviceManager::lastError() const {
    std::scoped_lock lock(mutex_);
    return lastError_;
}

std::optional<D3dError> D3d11DeviceManager::withImmediateContext(
    const ImmediateContextOperation& operation) {
    std::scoped_lock lock(mutex_);
    if (!current_ || !current_->device || !current_->immediateContext) {
        return D3dError{D3dErrorCode::DeviceLost, E_POINTER};
    }
    return operation(*current_->device, *current_->immediateContext);
}

bool D3d11DeviceManager::createAndPublish() {
    constexpr std::array drivers{
        D3dDriverKind::Hardware,
        D3dDriverKind::Warp,
    };

    std::optional<D3dError> failure;
    for (const auto driver : drivers) {
        auto result = factory_->create(driver);
        if (auto* created = std::get_if<D3dDeviceBundle>(&result)) {
            std::scoped_lock lock(mutex_);
            created->generation = generation_ + 1;
            current_ = std::move(*created);
            lastError_.reset();
            generation_ = current_->generation;
            return true;
        }
        failure = std::get<D3dError>(result);
    }

    std::scoped_lock lock(mutex_);
    lastError_ = failure;
    return false;
}

}  // namespace lc::graphics::d3d11
