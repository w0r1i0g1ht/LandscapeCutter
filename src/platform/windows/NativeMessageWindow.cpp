#include "platform/windows/NativeMessageWindow.hpp"

#include <QCoreApplication>
#include <QThread>

#include <cstddef>
#include <mutex>

namespace lc::platform::windows {
namespace {

constexpr wchar_t kWindowClassName[] = L"LandscapeCutter.NativeMessageWindow";

struct WindowClassState final {
    HINSTANCE instance = nullptr;
    std::size_t references = 0;
};

WindowClassState& windowClassState() {
    static WindowClassState state;
    return state;
}

std::mutex& windowClassMutex() {
    static std::mutex mutex;
    return mutex;
}

bool acquireWindowClass(const WNDPROC windowProc) {
    std::lock_guard lock(windowClassMutex());
    auto& state = windowClassState();
    if (state.references == 0) {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = instance;
        windowClass.lpszClassName = kWindowClassName;
        windowClass.lpfnWndProc = windowProc;

        if (RegisterClassExW(&windowClass) == 0) {
            return false;
        }

        state.instance = instance;
    }

    ++state.references;
    return true;
}

void releaseWindowClass() noexcept {
    std::lock_guard lock(windowClassMutex());
    auto& state = windowClassState();
    Q_ASSERT(state.references > 0);

    --state.references;
    if (state.references == 0) {
        UnregisterClassW(kWindowClassName, state.instance);
        state.instance = nullptr;
    }
}

}  // namespace

NativeMessageWindow::NativeMessageWindow(QObject* parent) : QObject(parent) {}

NativeMessageWindow::~NativeMessageWindow() {
    Q_ASSERT(QThread::currentThread() == thread());

    if (handle_ != nullptr) {
        DestroyWindow(handle_);
        handle_ = nullptr;
    }

    if (ownsWindowClassRegistration_) {
        releaseWindowClass();
    }
}

bool NativeMessageWindow::create() {
    Q_ASSERT(QThread::currentThread() == thread());
    if (handle_ != nullptr) {
        return true;
    }

    if (!acquireWindowClass(&NativeMessageWindow::windowProc)) {
        return false;
    }
    ownsWindowClassRegistration_ = true;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    handle_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWindowClassName, nullptr, WS_POPUP,
                              0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (handle_ == nullptr) {
        releaseWindowClass();
        ownsWindowClassRegistration_ = false;
        return false;
    }

    return true;
}

HWND NativeMessageWindow::handle() const noexcept {
    return handle_;
}

LRESULT CALLBACK NativeMessageWindow::windowProc(const HWND handle, const UINT message,
                                                  const WPARAM wParam, const LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(handle, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* window = reinterpret_cast<NativeMessageWindow*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    if (window != nullptr) {
        switch (message) {
            case WM_HOTKEY:
                emit window->hotkeyMessage(static_cast<int>(wParam));
                return 0;
            case WM_DISPLAYCHANGE:
            case WM_DEVICECHANGE:
                emit window->displayConfigurationChanged();
                return 0;
            case WM_CLOSE:
                QCoreApplication::quit();
                return 0;
            default:
                break;
        }
    }

    return DefWindowProcW(handle, message, wParam, lParam);
}

}  // namespace lc::platform::windows
