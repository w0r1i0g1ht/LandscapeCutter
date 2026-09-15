#include "app/AppController.hpp"
#include "app/AppMetadata.hpp"
#include "app/CaptureCoordinator.hpp"
#include "app/CaptureRuntimePolicy.hpp"
#include "app/LaunchOptions.hpp"
#include "capture/windows/MonitorCaptureService.hpp"
#include "graphics/d3d11/D3d11DeviceManager.hpp"
#include "platform/windows/DisplayCatalog.hpp"
#include "platform/windows/DpiAwareness.hpp"
#include "platform/windows/GlobalHotkeyService.hpp"
#include "platform/windows/NativeMessageWindow.hpp"
#include "platform/windows/SingleInstanceCoordinator.hpp"
#include "platform/windows/WindowsDisplayTopologySource.hpp"
#include "pin/PinManager.hpp"
#include "snip/SnapshotReadback.hpp"
#include "snip/SnipSession.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QtGui/qscreen_platform.h>

#include <Windows.h>

#include <winrt/base.h>

#include <string_view>
#include <limits>
#include <vector>

namespace {
lc::pin::ChoosePinSavePath pinSavePathChooser() {
    return [](std::function<void(QString)> accepted, std::function<void()> cancelled) {
        auto* dialog = new QFileDialog(nullptr, QStringLiteral("保存贴图"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setOption(QFileDialog::DontUseNativeDialog);
        dialog->setNameFilters(
            {QStringLiteral("PNG (*.png)"), QStringLiteral("JPEG (*.jpg *.jpeg)")});
        dialog->setDefaultSuffix(QStringLiteral("png"));
        dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
        dialog->selectFile(QStringLiteral("LandscapeCutter-Pin-%1.png")
                               .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
        QObject::connect(dialog, &QFileDialog::filterSelected, dialog,
                         [dialog](const QString& filter) {
                             const QString suffix = filter.startsWith(QStringLiteral("JPEG"))
                                                        ? QStringLiteral("jpg")
                                                        : QStringLiteral("png");
                             dialog->setDefaultSuffix(suffix);
                             const auto files = dialog->selectedFiles();
                             if (!files.isEmpty())
                                 dialog->selectFile(QFileInfo(files.front()).completeBaseName() +
                                                    '.' + suffix);
                         });
        QObject::connect(dialog, &QFileDialog::rejected, dialog,
                         [cancelled = std::move(cancelled)] { cancelled(); });
        QObject::connect(dialog, &QFileDialog::accepted, dialog,
                         [dialog, accepted = std::move(accepted)] {
                             const auto files = dialog->selectedFiles();
                             accepted(files.isEmpty() ? QString{} : files.front());
                         });
        dialog->open();
    };
}

QList<QRect> availableScreenGeometries() {
    QList<QRect> geometries;
    for (const auto* screen : QGuiApplication::screens()) {
        if (screen != nullptr)
            geometries.push_back(screen->availableGeometry());
    }
    return geometries;
}

QList<lc::pin::PinScreenGeometry> pinScreenGeometries(
    const std::vector<lc::platform::windows::MonitorDescriptor>& monitors) {
    QList<lc::pin::PinScreenGeometry> geometries;
    for (const auto& monitor : monitors) {
        const auto physicalWidth = lc::platform::width(monitor.desktopRect);
        const auto physicalHeight = lc::platform::height(monitor.desktopRect);
        if (physicalWidth <= 0 || physicalHeight <= 0 ||
            physicalWidth > std::numeric_limits<int>::max() ||
            physicalHeight > std::numeric_limits<int>::max()) {
            continue;
        }
        for (QScreen* screen : QGuiApplication::screens()) {
            const auto* native = screen->nativeInterface<QNativeInterface::QWindowsScreen>();
            if (native == nullptr || native->handle() != monitor.nativeHandle)
                continue;
            geometries.push_back(
                {{monitor.desktopRect.left, monitor.desktopRect.top,
                  static_cast<int>(physicalWidth), static_cast<int>(physicalHeight)},
                 screen->geometry(), screen->availableGeometry()});
            break;
        }
    }
    return geometries;
}
} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (lc::platform::windows::ensurePerMonitorV2().status ==
        lc::platform::windows::DpiSetupStatus::Failed) {
        MessageBoxW(nullptr, L"物理坐标模式不可用。请确认系统支持 Per-Monitor V2 DPI 感知后再试。",
                    L"LandscapeCutter", MB_OK | MB_ICONERROR);
        return 1;
    }

    lc::platform::windows::SingleInstanceCoordinator instanceCoordinator;
    const auto instance = instanceCoordinator.acquire();
    if (instance.role == lc::platform::windows::InstanceRole::Secondary) {
        instanceCoordinator.signalPrimary();
        return 0;
    }
    if (instance.role != lc::platform::windows::InstanceRole::Primary) {
        MessageBoxW(nullptr, L"无法初始化单实例协调器。", L"LandscapeCutter", MB_OK | MB_ICONERROR);
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
        QMessageBox::critical(nullptr, QStringLiteral("LandscapeCutter"),
                              QStringLiteral("应用图标资源不可用。请重新安装 LandscapeCutter "
                                             "或修复安装文件后再试。"));
        return 1;
    }

    lc::platform::windows::NativeMessageWindow nativeWindow;
    if (!nativeWindow.create() || !instanceCoordinator.beginListening()) {
        QMessageBox::critical(nullptr, QStringLiteral("LandscapeCutter"),
                              QStringLiteral("无法初始化应用消息窗口。"));
        return 1;
    }

    lc::platform::windows::WindowsDisplayTopologySource topologySource;
    lc::platform::windows::DisplayCatalog catalog(topologySource);
    const bool catalogReady =
        std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(
            catalog.refresh());

    lc::graphics::d3d11::D3d11DeviceManager deviceManager;
    const bool deviceReady = deviceManager.initialize();
    lc::capture::windows::MonitorCaptureService captureService(deviceManager);
    lc::app::CaptureCoordinator coordinator(captureService, deviceManager, [&catalog] {
        POINT cursor{};
        return GetCursorPos(&cursor) != FALSE ? catalog.monitorFromPoint(cursor) : std::nullopt;
    });
    lc::snip::SnapshotReadback readback(deviceManager);
    lc::snip::SnapshotBatch batch(captureService, deviceManager, readback.function());
    lc::pin::PinManager pinManager({}, pinSavePathChooser(),
                                   [&catalog] { return pinScreenGeometries(catalog.monitors()); });
    lc::snip::SnipSession snipSession(
        batch, {}, {},
        [&pinManager](std::unique_ptr<lc::annotation::AnnotationDocument>& document,
                      const QRect physicalSelection) {
            return pinManager.create(std::move(document), physicalSelection);
        });
    QObject::connect(&snipSession, &lc::snip::SnipSession::errorOccurred, &controller,
                     &lc::app::AppController::showErrorMessage);
    QObject::connect(&pinManager, &lc::pin::PinManager::errorOccurred, &controller,
                     &lc::app::AppController::showErrorMessage);
    QObject::connect(&pinManager, &lc::pin::PinManager::countChanged, &controller,
                     &lc::app::AppController::setPinCount);
    QObject::connect(&controller, &lc::app::AppController::closeAllPinsRequested, &pinManager,
                     &lc::pin::PinManager::closeAll);
    QObject::connect(&batch, &lc::snip::SnapshotBatch::failed, &coordinator,
                     [&coordinator](lc::app::CaptureNoticeCode code) {
                         if (code == lc::app::CaptureNoticeCode::DeviceRecoveryFailed ||
                             code == lc::app::CaptureNoticeCode::DeviceUnavailable) {
                             coordinator.setAvailability(
                                 lc::app::CaptureAvailability::DeviceUnavailable);
                         }
                     });
    lc::platform::windows::GlobalHotkeyService hotkey(nativeWindow);
    const bool supported = lc::capture::windows::MonitorCaptureService::isSupported();
    auto runtimeState =
        lc::app::CaptureRuntimePolicy::initial(supported, catalogReady, deviceReady);
    const auto applyRuntimeState = [&controller,
                                    &coordinator](lc::app::CaptureRuntimeState& state) {
        const auto pendingNotice = lc::app::CaptureRuntimePolicy::takePendingNotice(state);
        controller.setCaptureEnabled(state.captureEnabled);
        coordinator.setAvailability(state.availability);
        if (pendingNotice) {
            controller.showCaptureNotice(lc::app::CaptureNotice{.code = *pendingNotice});
        }
    };
    applyRuntimeState(runtimeState);
    if (runtimeState.registerHotkey) {
        runtimeState = lc::app::CaptureRuntimePolicy::afterHotkeyRegistration(
            runtimeState, hotkey.registerBinding({0x4C43, MOD_NOREPEAT, VK_F2}));
        applyRuntimeState(runtimeState);
        if (runtimeState.showHotkeyConflict) {
            controller.showHotkeyConflict();
        }
        const auto device = deviceManager.current();
        if (device && device->driverKind == lc::graphics::d3d11::D3dDriverKind::Warp) {
            controller.showCompatibilityMode();
        }
    }

    const auto beginSnip = [&snipSession, &catalog, &runtimeState] {
        if (runtimeState.captureEnabled)
            snipSession.begin(catalog.monitors());
    };
    QObject::connect(&controller, &lc::app::AppController::captureRequested, &snipSession,
                     beginSnip);
    QObject::connect(&hotkey, &lc::platform::windows::GlobalHotkeyService::activated, &snipSession,
                     beginSnip);
    QObject::connect(&coordinator, &lc::app::CaptureCoordinator::noticeReady, &controller,
                     &lc::app::AppController::showCaptureNotice);
    QObject::connect(
        &coordinator, &lc::app::CaptureCoordinator::availabilityChanged, &controller,
        [&controller, &hotkey, &runtimeState](const lc::app::CaptureAvailability availability) {
            runtimeState =
                lc::app::CaptureRuntimePolicy::afterAvailabilityChanged(runtimeState, availability);
            controller.setCaptureEnabled(runtimeState.captureEnabled);
            if (!runtimeState.captureEnabled) {
                hotkey.unregister();
            }
        });
    QObject::connect(&instanceCoordinator,
                     &lc::platform::windows::SingleInstanceCoordinator::activationRequested,
                     &controller, &lc::app::AppController::showAlreadyRunning);

    QTimer refreshTimer;
    refreshTimer.setSingleShot(true);
    QObject::connect(
        &nativeWindow, &lc::platform::windows::NativeMessageWindow::displayConfigurationChanged,
        &refreshTimer, [&refreshTimer, &snipSession, &runtimeState, &applyRuntimeState, &hotkey] {
            snipSession.cancel();
            runtimeState = lc::app::CaptureRuntimePolicy::afterDisplayInvalidated(runtimeState);
            applyRuntimeState(runtimeState);
            hotkey.unregister();
            refreshTimer.start(100);
        });
    QObject::connect(
        &refreshTimer, &QTimer::timeout, &controller,
        [&catalog, &controller, &coordinator, &hotkey, &runtimeState, &applyRuntimeState,
         &snipSession, &pinManager] {
            snipSession.cancel();
            const bool refreshed =
                std::holds_alternative<std::vector<lc::platform::windows::MonitorDescriptor>>(
                    catalog.refresh());
            if (refreshed)
                pinManager.recoverVisibility(availableScreenGeometries());
            runtimeState =
                lc::app::CaptureRuntimePolicy::afterDisplayRefresh(runtimeState, refreshed);
            const bool needsHotkeyRegistration = runtimeState.registerHotkey;
            applyRuntimeState(runtimeState);
            if (needsHotkeyRegistration && !hotkey.isRegistered()) {
                runtimeState = lc::app::CaptureRuntimePolicy::afterHotkeyRegistration(
                    runtimeState, hotkey.registerBinding({0x4C43, MOD_NOREPEAT, VK_F2}));
                applyRuntimeState(runtimeState);
                if (runtimeState.showHotkeyConflict) {
                    controller.showHotkeyConflict();
                }
            }
            if (!runtimeState.captureEnabled) {
                hotkey.unregister();
            }
        });
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &application,
                     [&coordinator, &hotkey, &snipSession, &pinManager] {
                         snipSession.cancel();
                         pinManager.closeAll();
                         coordinator.shutdown();
                         hotkey.unregister();
                     });

    return application.exec();
}
