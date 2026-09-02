#include "app/AppController.hpp"

#include <QApplication>
#include <QSystemTrayIcon>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("controller starts when the system tray is initially unavailable") {
    int argc = 1;
    char applicationName[] = "landscapecutter_app_controller_tests";
    char* argv[] = {applicationName, nullptr};
    QApplication application(argc, argv);

    REQUIRE_FALSE(QSystemTrayIcon::isSystemTrayAvailable());

    lc::app::AppController controller(application);
    CHECK(controller.start());
}
