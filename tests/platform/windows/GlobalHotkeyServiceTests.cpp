#include "platform/windows/GlobalHotkeyService.hpp"
#include "platform/windows/NativeMessageWindow.hpp"

#include <Windows.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <catch2/catch_test_macros.hpp>

namespace {

bool processEventsUntil(const int& activationCount) {
    QElapsedTimer timer;
    timer.start();

    while (activationCount == 0 && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        Sleep(1);
    }

    return activationCount == 1;
}

}  // namespace

TEST_CASE("global hotkey registration reports conflicts and forwards one activation") {
    lc::platform::windows::NativeMessageWindow firstWindow;
    lc::platform::windows::NativeMessageWindow secondWindow;
    REQUIRE(firstWindow.create());
    REQUIRE(secondWindow.create());

    lc::platform::windows::GlobalHotkeyService firstService(firstWindow);
    lc::platform::windows::GlobalHotkeyService secondService(secondWindow);
    constexpr lc::platform::windows::HotkeyBinding testBinding{
        0x7A32, MOD_NOREPEAT, VK_F24};

    CHECK(firstService.registerBinding(testBinding) ==
          lc::platform::windows::HotkeyRegistrationStatus::Registered);
    CHECK(firstService.isRegistered());
    CHECK(firstService.registerBinding(testBinding) ==
          lc::platform::windows::HotkeyRegistrationStatus::Registered);
    CHECK(secondService.registerBinding(testBinding) ==
          lc::platform::windows::HotkeyRegistrationStatus::Conflict);
    CHECK_FALSE(secondService.isRegistered());

    int activationCount = 0;
    QObject::connect(&firstService, &lc::platform::windows::GlobalHotkeyService::activated,
                     [&activationCount] { ++activationCount; });
    REQUIRE(PostMessageW(firstWindow.handle(), WM_HOTKEY,
                         static_cast<WPARAM>(testBinding.id), 0) != FALSE);
    REQUIRE(processEventsUntil(activationCount));

    firstService.unregister();
    CHECK_FALSE(firstService.isRegistered());
    CHECK(secondService.registerBinding(testBinding) ==
          lc::platform::windows::HotkeyRegistrationStatus::Registered);
    CHECK(secondService.isRegistered());
}
