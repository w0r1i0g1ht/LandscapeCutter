#include "app/AppController.hpp"
#include "app/AppMetadata.hpp"
#include "app/LaunchOptions.hpp"
#include "platform/windows/DpiAwareness.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>

#include <Windows.h>

#include <winrt/base.h>

#include <string_view>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (lc::platform::windows::ensurePerMonitorV2().status ==
        lc::platform::windows::DpiSetupStatus::Failed) {
        MessageBoxW(nullptr,
                    L"物理坐标模式不可用。请确认系统支持 Per-Monitor V2 DPI 感知后再试。",
                    L"LandscapeCutter",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QString::fromUtf8(lc::app::AppMetadata::name.data(),
                          static_cast<qsizetype>(lc::app::AppMetadata::name.size())));
    QCoreApplication::setOrganizationName(
        QString::fromUtf8(lc::app::AppMetadata::organization.data(),
                          static_cast<qsizetype>(lc::app::AppMetadata::organization.size())));
    QCoreApplication::setApplicationVersion(
        QString::fromUtf8(lc::app::AppMetadata::version.data(),
                          static_cast<qsizetype>(lc::app::AppMetadata::version.size())));
    application.setQuitOnLastWindowClosed(false);

    if (lc::app::parseLaunchMode(arguments) == lc::app::LaunchMode::SmokeTest) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
        return application.exec();
    }

    lc::app::AppController controller(application);
    if (!controller.start()) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("LandscapeCutter"),
                              QStringLiteral("应用图标资源不可用。请重新安装 LandscapeCutter "
                                             "或修复安装文件后再试。"));
        return 1;
    }

    return application.exec();
}
