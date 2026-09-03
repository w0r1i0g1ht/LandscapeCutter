#include "platform/windows/GlobalHotkeyService.hpp"

#include "platform/windows/NativeMessageWindow.hpp"

#include <Windows.h>

namespace lc::platform::windows {

GlobalHotkeyService::GlobalHotkeyService(NativeMessageWindow& window, QObject* parent)
    : QObject(parent), window_(window) {
    connect(&window_, &NativeMessageWindow::hotkeyMessage, this,
            [this](const int id) {
                if (binding_.has_value() && binding_->id == id) {
                    emit activated();
                }
            });
}

GlobalHotkeyService::~GlobalHotkeyService() {
    unregister();
}

HotkeyRegistrationStatus GlobalHotkeyService::registerBinding(const HotkeyBinding binding) {
    unregister();

    if (window_.handle() == nullptr) {
        return HotkeyRegistrationStatus::Failed;
    }

    if (RegisterHotKey(window_.handle(), binding.id, binding.modifiers, binding.virtualKey) != FALSE) {
        binding_ = binding;
        return HotkeyRegistrationStatus::Registered;
    }

    return GetLastError() == ERROR_HOTKEY_ALREADY_REGISTERED
               ? HotkeyRegistrationStatus::Conflict
               : HotkeyRegistrationStatus::Failed;
}

void GlobalHotkeyService::unregister() noexcept {
    if (!binding_.has_value()) {
        return;
    }

    UnregisterHotKey(window_.handle(), binding_->id);
    binding_.reset();
}

bool GlobalHotkeyService::isRegistered() const noexcept {
    return binding_.has_value();
}

}  // namespace lc::platform::windows
