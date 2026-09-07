#pragma once

#include "capture/CaptureFrame.hpp"
#include "capture/MonitorCaptureService.hpp"
#include "graphics/d3d11/D3d11DeviceManager.hpp"

#include <QObject>
#include <QMetaType>
#include <QPointer>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

namespace lc::app {

enum class CaptureState { Idle, Capturing };
enum class CaptureAvailability { Available, Unsupported, DisplayUnavailable, DeviceUnavailable };
enum class CaptureNoticeCode {
    Success,
    Busy,
    HotkeyConflict,
    Unsupported,
    MonitorUnavailable,
    AccessDenied,
    DisplayUnavailable,
    Timeout,
    InvalidFrame,
    DisplayChanged,
    DeviceUnavailable,
    DeviceRecoveryFailed,
    CaptureCancelled,
    InternalFailure,
};

struct CaptureNotice final {
    CaptureNoticeCode code;
    std::optional<platform::MonitorId> monitor;
    std::wstring displayName;
    std::optional<platform::PixelSize> size;
    std::optional<capture::CapturePixelFormat> pixelFormat;
    std::chrono::milliseconds elapsed;
};

using MonitorSelector =
    std::function<std::optional<platform::windows::MonitorDescriptor>()>;

class CaptureCoordinator final : public QObject {
    Q_OBJECT

public:
    CaptureCoordinator(capture::IMonitorCaptureService& captureService,
                       graphics::d3d11::ID3d11DeviceRecovery& deviceRecovery,
                       MonitorSelector monitorSelector,
                       QObject* parent = nullptr);
    ~CaptureCoordinator() override;

    void requestCapture();
    void setAvailability(CaptureAvailability availability);
    void shutdown() noexcept;
    CaptureState state() const noexcept;
    const std::optional<capture::CaptureFrame>& latestFrame() const noexcept;

signals:
    void noticeReady(const CaptureNotice& notice);
    void availabilityChanged(CaptureAvailability availability);

private:
    struct ActiveCapture final {
        platform::windows::MonitorDescriptor monitor;
        std::chrono::steady_clock::time_point started;
        std::uint64_t displayGeneration;
        std::uint64_t deviceGeneration;
        std::uint64_t id;
        std::uint32_t retryCount{};
        std::uint32_t attempt{};
    };

    void startAttempt();
    void complete(std::uint64_t captureId, std::uint32_t attempt,
                  capture::CaptureResult result);
    void finish(CaptureNoticeCode code,
                std::optional<CaptureAvailability> availability = std::nullopt);
    void setAvailabilityInternal(CaptureAvailability availability);
    CaptureNotice notice(CaptureNoticeCode code) const;

    capture::IMonitorCaptureService& captureService_;
    graphics::d3d11::ID3d11DeviceRecovery& deviceRecovery_;
    MonitorSelector monitorSelector_;
    CaptureAvailability availability_{CaptureAvailability::Available};
    CaptureState state_{CaptureState::Idle};
    std::optional<capture::CaptureFrame> latestFrame_;
    std::optional<ActiveCapture> active_;
    std::uint64_t nextCaptureId_{};
    bool shuttingDown_{};
};

}  // namespace lc::app

Q_DECLARE_METATYPE(lc::app::CaptureAvailability)
Q_DECLARE_METATYPE(lc::app::CaptureNotice)
