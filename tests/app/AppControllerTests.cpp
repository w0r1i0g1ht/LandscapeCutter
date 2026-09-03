#include "app/AppController.hpp"

#include <QApplication>
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
