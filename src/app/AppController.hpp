#pragma once

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class QApplication;

namespace lc::app {

class AppController final : public QObject {
public:
    explicit AppController(QApplication& application);

    [[nodiscard]] bool start();

private:
    QApplication& application_;
    QMenu trayMenu_;
    QAction quitAction_;
    QSystemTrayIcon trayIcon_;
};

} // namespace lc::app
