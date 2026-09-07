#include "app/AppController.hpp"

#include <QApplication>
#include <QIcon>

#include <spdlog/spdlog.h>

namespace lc::app {

QString formatCaptureNotice(const CaptureNotice& notice) {
    if (notice.code == CaptureNoticeCode::Success && notice.size && notice.pixelFormat) {
        const auto name = QString::fromWCharArray(notice.displayName.c_str());
        const auto format = *notice.pixelFormat == capture::CapturePixelFormat::Bgra8Unorm
                                ? QStringLiteral("SDR BGRA8")
                                : QStringLiteral("HDR RGBA16F");
        return QStringLiteral("已捕获 %1：%2 × %3（%4）")
            .arg(name)
            .arg(notice.size->width)
            .arg(notice.size->height)
            .arg(format);
    }

    switch (notice.code) {
    case CaptureNoticeCode::Busy: return QStringLiteral("捕获正在进行，请稍候。");
    case CaptureNoticeCode::HotkeyConflict: return QStringLiteral("F2 已被其他程序占用，请从托盘菜单捕获。");
    case CaptureNoticeCode::Unsupported: return QStringLiteral("此系统不支持 Windows 图形捕获。");
    case CaptureNoticeCode::MonitorUnavailable:
    case CaptureNoticeCode::DisplayUnavailable: return QStringLiteral("当前显示器不可用，请刷新后重试。");
    case CaptureNoticeCode::AccessDenied: return QStringLiteral("系统拒绝了捕获请求。");
    case CaptureNoticeCode::Timeout: return QStringLiteral("捕获超时，请重试。");
    case CaptureNoticeCode::InvalidFrame: return QStringLiteral("未取得有效帧，请重试。");
    case CaptureNoticeCode::DisplayChanged: return QStringLiteral("显示配置已变化，请重试。");
    case CaptureNoticeCode::DeviceUnavailable:
    case CaptureNoticeCode::DeviceRecoveryFailed: return QStringLiteral("图形设备不可用。");
    case CaptureNoticeCode::CaptureCancelled: return QStringLiteral("捕获已取消。");
    case CaptureNoticeCode::InternalFailure: return QStringLiteral("捕获失败，请重试。");
    case CaptureNoticeCode::Success: return QStringLiteral("捕获完成。");
    }
    return QStringLiteral("捕获失败，请重试。");
}

AppController::AppController(QApplication& application)
    : application_(application),
      captureAction_(tr("捕获当前显示器单帧"), this),
      quitAction_(tr("退出")) {
    captureAction_.setObjectName(QStringLiteral("captureCurrentMonitorAction"));
    trayMenu_.addAction(&quitAction_);
    trayMenu_.insertAction(&quitAction_, &captureAction_);
    trayIcon_.setContextMenu(&trayMenu_);

    connect(&quitAction_, &QAction::triggered, &application_, &QCoreApplication::quit);
    connect(&captureAction_, &QAction::triggered, this, &AppController::captureRequested);
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

void AppController::setCaptureEnabled(const bool enabled) {
    captureAction_.setEnabled(enabled);
}

void AppController::showCaptureNotice(const CaptureNotice& notice) {
    trayIcon_.showMessage(QStringLiteral("LandscapeCutter"), formatCaptureNotice(notice),
                          notice.code == CaptureNoticeCode::Success
                              ? QSystemTrayIcon::Information
                              : QSystemTrayIcon::Warning,
                          3000);
}

void AppController::showAlreadyRunning() {
    trayIcon_.showMessage(QStringLiteral("LandscapeCutter"), QStringLiteral("LandscapeCutter 已在运行。"),
                          QSystemTrayIcon::Information, 3000);
}

void AppController::showHotkeyConflict() {
    trayIcon_.showMessage(QStringLiteral("LandscapeCutter"),
                          QStringLiteral("F2 已被其他程序占用，请从托盘菜单捕获。"),
                          QSystemTrayIcon::Warning, 3000);
}

void AppController::showCompatibilityMode() {
    trayIcon_.showMessage(QStringLiteral("LandscapeCutter"),
                          QStringLiteral("硬件图形设备不可用，已使用兼容模式。"),
                          QSystemTrayIcon::Information, 3000);
}

} // namespace lc::app
