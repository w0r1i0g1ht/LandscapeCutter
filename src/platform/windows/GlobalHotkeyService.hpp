#pragma once

#include <QObject>

#include <optional>

namespace lc::platform::windows {

class NativeMessageWindow;

struct HotkeyBinding final {
    int id;
    unsigned int modifiers;
    unsigned int virtualKey;
};

enum class HotkeyRegistrationStatus { Registered, Conflict, Failed };

class GlobalHotkeyService final : public QObject {
    Q_OBJECT

public:
    explicit GlobalHotkeyService(NativeMessageWindow& window, QObject* parent = nullptr);
    ~GlobalHotkeyService() override;

    HotkeyRegistrationStatus registerBinding(HotkeyBinding binding);
    void unregister() noexcept;
    bool isRegistered() const noexcept;

signals:
    void activated();

private:
    NativeMessageWindow& window_;
    std::optional<HotkeyBinding> binding_;
};

}  // namespace lc::platform::windows
