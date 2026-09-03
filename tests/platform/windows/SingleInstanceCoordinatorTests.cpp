#include "platform/windows/SingleInstanceCoordinator.hpp"

#include <Windows.h>
#include <objbase.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <functional>
#include <string>

namespace {

std::wstring uniqueNamespaceSuffix() {
    GUID guid{};
    REQUIRE(CoCreateGuid(&guid) == S_OK);

    std::array<wchar_t, 39> guidText{};
    REQUIRE(StringFromGUID2(guid, guidText.data(), static_cast<int>(guidText.size())) != 0);
    return L".Tests" + std::wstring{guidText.data()};
}

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

TEST_CASE("the first coordinator is primary and the second is secondary") {
    const auto suffix = uniqueNamespaceSuffix();
    lc::platform::windows::SingleInstanceCoordinator first{suffix};
    lc::platform::windows::SingleInstanceCoordinator second{suffix};

    CHECK(first.acquire().role == lc::platform::windows::InstanceRole::Primary);
    CHECK(second.acquire().role == lc::platform::windows::InstanceRole::Secondary);
}

TEST_CASE("a secondary coordinator wakes the primary exactly once") {
    const auto suffix = uniqueNamespaceSuffix();
    lc::platform::windows::SingleInstanceCoordinator primary{suffix};
    lc::platform::windows::SingleInstanceCoordinator secondary{suffix};

    REQUIRE(primary.acquire().role == lc::platform::windows::InstanceRole::Primary);
    REQUIRE(secondary.acquire().role == lc::platform::windows::InstanceRole::Secondary);
    REQUIRE(secondary.signalPrimary());

    int activationCount = 0;
    QObject::connect(&primary,
                     &lc::platform::windows::SingleInstanceCoordinator::activationRequested,
                     [&activationCount] { ++activationCount; });
    REQUIRE(primary.beginListening());
    REQUIRE(processEventsUntil([&activationCount] { return activationCount == 1; }));

    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    CHECK(activationCount == 1);
}

TEST_CASE("a new coordinator becomes primary after all handles close") {
    const auto suffix = uniqueNamespaceSuffix();

    {
        lc::platform::windows::SingleInstanceCoordinator first{suffix};
        lc::platform::windows::SingleInstanceCoordinator second{suffix};
        REQUIRE(first.acquire().role == lc::platform::windows::InstanceRole::Primary);
        REQUIRE(second.acquire().role == lc::platform::windows::InstanceRole::Secondary);
    }

    lc::platform::windows::SingleInstanceCoordinator replacement{suffix};
    CHECK(replacement.acquire().role == lc::platform::windows::InstanceRole::Primary);
}

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    Catch::Session session;
    return session.run(argc, argv);
}
