#pragma once

#include "capture/MonitorCaptureService.hpp"
#include "graphics/d3d11/D3d11DeviceManager.hpp"

#include <QObject>
#include <memory>
#include <vector>

namespace lc::capture::windows {

struct CapturePoolOptions final {
    platform::PixelSize size{};
    DXGI_FORMAT format{};
    int bufferCount{2};
    std::chrono::milliseconds timeout{2000};
};

struct IncomingCaptureFrame final {
    winrt::com_ptr<ID3D11Texture2D> texture;
    std::int32_t contentWidth{};
    std::int32_t contentHeight{};
    std::optional<std::chrono::nanoseconds> systemRelativeTime;
    // Keeps the borrowed WGC frame open until validation and copyOwned finish.
    std::shared_ptr<void> lifetime;
};

using FrameReader = std::function<IncomingCaptureFrame()>;
using FrameHandler = std::function<void(const FrameReader&)>;

// Narrow producer boundary: handlers invoke readers synchronously after entering
// the callback gate. revoke() stops subscription, then the service drains readers
// before close(). Both functions must tolerate partially completed start().
class IMonitorFrameSource {
public:
    virtual ~IMonitorFrameSource() = default;
    virtual bool isSupported() = 0;
    virtual void start(HMONITOR monitor, const graphics::d3d11::D3dDeviceBundle& device,
                       const CapturePoolOptions& options, FrameHandler handler) = 0;
    virtual void revoke() noexcept = 0;
    virtual void close() noexcept = 0;
};
using CaptureSourceFactory = std::function<std::unique_ptr<IMonitorFrameSource>()>;

class MonitorCaptureService final : public QObject, public IMonitorCaptureService {
public:
    static bool isSupported() noexcept;
    explicit MonitorCaptureService(graphics::d3d11::D3d11DeviceManager& manager,
                                   QObject* parent = nullptr);
    MonitorCaptureService(graphics::d3d11::D3d11DeviceManager& manager,
                          CaptureSourceFactory factory, QObject* parent = nullptr);
    ~MonitorCaptureService() override;

    // Called and destroyed on the Qt main thread. A duplicate request receives
    // Cancelled without opening or queuing a second WGC session.
    void captureOnce(MonitorCaptureRequest request, CaptureCompletion completion) override;
    void cancel() noexcept override;

private:
    struct Operation;
    void queueFinalization(const std::shared_ptr<Operation>& operation);
    void finalize(const std::shared_ptr<Operation>& operation) noexcept;
    CaptureResult readAndCopy(Operation& operation, const FrameReader& readFrame);

    graphics::d3d11::D3d11DeviceManager& manager_;
    CaptureSourceFactory factory_;
    std::shared_ptr<Operation> active_;
    // These hold only already-rejected completion records, never pending captures.
    std::vector<std::shared_ptr<Operation>> rejectedCompletions_;
};

}  // namespace lc::capture::windows
