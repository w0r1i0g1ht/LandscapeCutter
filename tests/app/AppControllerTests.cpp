#include "app/AppController.hpp"
#include "app/CaptureCoordinator.hpp"

#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("controller honors the icon resource boundary when the tray is initially unavailable") {
    int argc = 1;
    char applicationName[] = "landscapecutter_app_controller_tests";
    char* argv[] = {applicationName, nullptr};
    QApplication application(argc, argv);

    REQUIRE_FALSE(QSystemTrayIcon::isSystemTrayAvailable());

    Q_CLEANUP_RESOURCE(resources);
    lc::app::AppController missingResourceController(application);
    CHECK_FALSE(missingResourceController.start());

    Q_INIT_RESOURCE(resources);
    lc::app::AppController availableResourceController(application);
    CHECK(availableResourceController.start());
}

TEST_CASE("the tray menu exposes capture before quit") {
    int argc = 1;
    char applicationName[] = "landscapecutter_app_controller_menu_tests";
    char* argv[] = {applicationName, nullptr};
    QApplication application(argc, argv);

    lc::app::AppController controller(application);
    const auto* capture = controller.findChild<QAction*>("captureCurrentMonitorAction");
    const auto associated = capture == nullptr ? QList<QObject*>{} : capture->associatedObjects();
    const auto menuObject = std::find_if(associated.cbegin(), associated.cend(),
                                         [](const QObject* object) {
                                             return qobject_cast<const QMenu*>(object) != nullptr;
                                         });

    REQUIRE(capture != nullptr);
    REQUIRE(menuObject != associated.cend());
    const auto* menu = qobject_cast<const QMenu*>(*menuObject);
    REQUIRE(menu->actions().size() == 2);
    CHECK(menu->actions().at(0) == capture);
    CHECK(menu->actions().at(1)->text() == QStringLiteral("退出"));
}

TEST_CASE("capture availability enables and disables the capture action") {
    int argc = 1;
    char applicationName[] = "landscapecutter_app_controller_availability_tests";
    char* argv[] = {applicationName, nullptr};
    QApplication application(argc, argv);

    lc::app::AppController controller(application);
    auto* capture = controller.findChild<QAction*>("captureCurrentMonitorAction");
    REQUIRE(capture != nullptr);

    controller.setCaptureEnabled(false);
    CHECK_FALSE(capture->isEnabled());
    controller.setCaptureEnabled(true);
    CHECK(capture->isEnabled());
}

TEST_CASE("triggering the tray capture action emits one capture request") {
    int argc = 1;
    char applicationName[] = "landscapecutter_app_controller_trigger_tests";
    char* argv[] = {applicationName, nullptr};
    QApplication application(argc, argv);

    lc::app::AppController controller(application);
    auto* capture = controller.findChild<QAction*>("captureCurrentMonitorAction");
    REQUIRE(capture != nullptr);
    int requests{};
    QObject::connect(&controller, &lc::app::AppController::captureRequested,
                     [&requests] { ++requests; });

    capture->trigger();

    CHECK(requests == 1);
}

TEST_CASE("structured success notices include monitor size and pixel format") {
    const lc::app::CaptureNotice notice{
        .code = lc::app::CaptureNoticeCode::Success,
        .monitor = std::nullopt,
        .displayName = L"Fixture display",
        .size = lc::platform::PixelSize{1920, 1080},
        .pixelFormat = lc::capture::CapturePixelFormat::Bgra8Unorm,
        .elapsed = {},
    };

    const QString formatted = lc::app::formatCaptureNotice(notice);

    CHECK(formatted.contains(QStringLiteral("Fixture display")));
    CHECK(formatted.contains(QStringLiteral("1920")));
    CHECK(formatted.contains(QStringLiteral("1080")));
    CHECK(formatted.contains(QStringLiteral("SDR BGRA8")));
}

TEST_CASE("startup capture prerequisites have distinct user-facing notices") {
    const auto unsupported = lc::app::formatCaptureNotice(
        lc::app::CaptureNotice{.code = lc::app::CaptureNoticeCode::Unsupported});
    const auto displayUnavailable = lc::app::formatCaptureNotice(
        lc::app::CaptureNotice{.code = lc::app::CaptureNoticeCode::DisplayUnavailable});
    const auto deviceUnavailable = lc::app::formatCaptureNotice(
        lc::app::CaptureNotice{.code = lc::app::CaptureNoticeCode::DeviceUnavailable});

    CHECK(unsupported.contains(QStringLiteral("不支持")));
    CHECK(displayUnavailable.contains(QStringLiteral("显示器不可用")));
    CHECK(deviceUnavailable.contains(QStringLiteral("图形设备不可用")));
    CHECK(unsupported != displayUnavailable);
    CHECK(displayUnavailable != deviceUnavailable);
    CHECK(deviceUnavailable != unsupported);
}

TEST_CASE("an F2 hotkey conflict keeps tray capture enabled and reports the conflict") {
    int argc = 1;
    char applicationName[] = "landscapecutter_app_controller_hotkey_tests";
    char* argv[] = {applicationName, nullptr};
    QApplication application(argc, argv);

    lc::app::AppController controller(application);
    auto* capture = controller.findChild<QAction*>("captureCurrentMonitorAction");
    REQUIRE(capture != nullptr);
    controller.setCaptureEnabled(true);

    controller.showHotkeyConflict();

    CHECK(capture->isEnabled());
}
