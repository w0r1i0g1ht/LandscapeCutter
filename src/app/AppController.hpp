#pragma once

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

#include "app/CaptureCoordinator.hpp"

class QApplication;

namespace lc::app {

class AppController final : public QObject {
    Q_OBJECT

  public:
    explicit AppController(QApplication& application);

    [[nodiscard]] bool start();
    void setCaptureEnabled(bool enabled);
    void showCaptureNotice(const CaptureNotice& notice);
    void showErrorMessage(const QString& message);
    void showAlreadyRunning();
    void showHotkeyConflict();
    void showCompatibilityMode();

  signals:
    void captureRequested();

  private:
    QApplication& application_;
    QMenu trayMenu_;
    QAction captureAction_;
    QAction quitAction_;
    QSystemTrayIcon trayIcon_;
};

[[nodiscard]] QString formatCaptureNotice(const CaptureNotice& notice);

} // namespace lc::app
