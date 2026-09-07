#include <Windows.h>
#include <wil/resource.h>

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 3) { return 1; }
    wil::unique_handle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[1]));
    wil::unique_handle stop(OpenEventW(SYNCHRONIZE, FALSE, argv[2]));
    if (!ready || !stop) { return 1; }
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC",
                                 L"LandscapeCutter test hotkey holder", WS_POPUP,
                                 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) { return 1; }
    if (!RegisterHotKey(window, 1, MOD_NOREPEAT, VK_F2)) {
        const auto code = GetLastError();
        DestroyWindow(window);
        return code == ERROR_HOTKEY_ALREADY_REGISTERED ? 125 : 1;
    }
    int result = SetEvent(ready.get()) ? 0 : 1;
    const HANDLE event = stop.get();
    while (result == 0) {
        const DWORD wait = MsgWaitForMultipleObjects(1, &event, FALSE, 30000, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) { break; }
        if (wait != WAIT_OBJECT_0 + 1) { result = 1; break; }
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    UnregisterHotKey(window, 1);
    DestroyWindow(window);
    return result;
}
