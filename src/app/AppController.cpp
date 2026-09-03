#include "app/AppController.hpp"

#include <QApplication>
#include <QIcon>

#include <spdlog/spdlog.h>

namespace lc::app {

AppController::AppController(QApplication& application)
    : application_(application), quitAction_(tr("退出")) {
    trayMenu_.addAction(&quitAction_);
    trayIcon_.setContextMenu(&trayMenu_);

    connect(&quitAction_, &QAction::triggered, &application_, &QCoreApplication::quit);
}

bool AppController::start() {
    const QIcon icon(QStringLiteral(":/icons/LandscapeCutter.ico"));
    if (icon.isNull()) {
        spdlog::error("Application icon resource is unavailable");
        return false;
    }

    trayIcon_.setIcon(icon);
    trayIcon_.setToolTip(QStringLiteral("LandscapeCutter"));
    trayIcon_.show();
    trayIcon_.showMessage(QStringLiteral("LandscapeCutter"),
                          QStringLiteral("C++ foundation is running."),
                          QSystemTrayIcon::Information,
                          2000);
    return true;
}

} // namespace lc::app
