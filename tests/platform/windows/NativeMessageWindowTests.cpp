#define CATCH_CONFIG_RUNNER

#include "platform/windows/NativeMessageWindow.hpp"

#include <Windows.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <functional>

namespace {

bool processEventsUntil(const std::function<bool()>& predicate) {
    QElapsedTimer timer;
    timer.start();

    while (!predicate() && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        Sleep(1);
    }

    return predicate();
}

}  // namespace

TEST_CASE("native message window is an invisible top-level tool window") {
    lc::platform::windows::NativeMessageWindow window;

    REQUIRE(window.create());

    const HWND handle = window.handle();
    CHECK(IsWindow(handle) != FALSE);
    CHECK(IsWindowVisible(handle) == FALSE);
    CHECK(GetParent(handle) != HWND_MESSAGE);
    CHECK((GetWindowLongPtrW(handle, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0);
}

TEST_CASE("native message window forwards hotkey ids") {
    lc::platform::windows::NativeMessageWindow window;
    REQUIRE(window.create());

    int receivedId = 0;
    QObject::connect(&window, &lc::platform::windows::NativeMessageWindow::hotkeyMessage,
                     [&receivedId](const int id) { receivedId = id; });

    constexpr int hotkeyId = 0x7A31;
    REQUIRE(PostMessageW(window.handle(), WM_HOTKEY, static_cast<WPARAM>(hotkeyId), 0) != FALSE);
    REQUIRE(processEventsUntil([&receivedId] { return receivedId == hotkeyId; }));
}

TEST_CASE("native message window forwards display change broadcasts") {
    lc::platform::windows::NativeMessageWindow window;
    REQUIRE(window.create());

    int receivedCount = 0;
    QObject::connect(&window, &lc::platform::windows::NativeMessageWindow::displayConfigurationChanged,
                     [&receivedCount] { ++receivedCount; });

    REQUIRE(PostMessageW(window.handle(), WM_DISPLAYCHANGE, 0, 0) != FALSE);
    REQUIRE(PostMessageW(window.handle(), WM_DEVICECHANGE, 0, 0) != FALSE);
    REQUIRE(processEventsUntil([&receivedCount] { return receivedCount == 2; }));
}

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    Catch::Session session;
    return session.run(argc, argv);
}
