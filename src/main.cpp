#include "app/AppController.hpp"
#include "app/AppMetadata.hpp"
#include "app/LaunchOptions.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QTimer>

#include <string_view>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
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
                              QStringLiteral("系统托盘不可用，程序无法启动。"));
        return 1;
    }

    return application.exec();
}
