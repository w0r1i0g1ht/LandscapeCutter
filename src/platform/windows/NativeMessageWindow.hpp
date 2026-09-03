#pragma once

#include <Windows.h>

#include <QObject>

namespace lc::platform::windows {

class NativeMessageWindow final : public QObject {
    Q_OBJECT

public:
    explicit NativeMessageWindow(QObject* parent = nullptr);
    ~NativeMessageWindow() override;

    bool create();
    HWND handle() const noexcept;

signals:
    void hotkeyMessage(int id);
    void displayConfigurationChanged();

private:
    static LRESULT CALLBACK windowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);

    HWND handle_ = nullptr;
    bool ownsWindowClassRegistration_ = false;
};

}  // namespace lc::platform::windows
