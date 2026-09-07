#include "capture/windows/MonitorCaptureService.hpp"

#include "capture/windows/GraphicsCaptureInterop.hpp"
#include "capture/windows/OneShotCaptureState.hpp"
#include "graphics/d3d11/TextureCopy.hpp"

#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <wil/resource.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.h>

#include <limits>
#include <utility>

namespace lc::capture::windows {
namespace {
using namespace winrt::Windows::Graphics::Capture;
using namespace graphics::d3d11;

CaptureError nativeError(HRESULT nativeCode) noexcept {
    CaptureErrorCode code = CaptureErrorCode::Internal;
    switch (nativeCode) {
    case E_ACCESSDENIED: code = CaptureErrorCode::AccessDenied; break;
    case E_INVALIDARG:
    case HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE):
    case HRESULT_FROM_WIN32(ERROR_NOT_FOUND): code = CaptureErrorCode::MonitorUnavailable; break;
    case HRESULT_FROM_WIN32(ERROR_RETRY): code = CaptureErrorCode::DisplayChanged; break;
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET:
    case DXGI_ERROR_DEVICE_HUNG:
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR: code = CaptureErrorCode::DeviceLost; break;
    case E_NOTIMPL:
    case REGDB_E_CLASSNOTREG:
    case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED): code = CaptureErrorCode::Unsupported; break;
    default: break;
    }
    return {code, {static_cast<int>(nativeCode), std::system_category()}};
}

struct WgcFrameLease final {
    explicit WgcFrameLease(Direct3D11CaptureFrame value) : frame(std::move(value)) {}
    ~WgcFrameLease() { try { frame.Close(); } catch (...) {} }
    Direct3D11CaptureFrame frame;
};

class WgcMonitorFrameSource final : public IMonitorFrameSource {
public:
    ~WgcMonitorFrameSource() override { revoke(); close(); }
    bool isSupported() override { return GraphicsCaptureSession::IsSupported(); }

    void start(HMONITOR monitor, const D3dDeviceBundle& device,
               const CapturePoolOptions& options, FrameHandler handler) override {
        item_ = createItemForMonitor(monitor);
        const auto size = item_.Size();
        if (size.Width != static_cast<int>(options.size.width) ||
            size.Height != static_cast<int>(options.size.height)) {
            winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_RETRY));
        }
        using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
        const auto format = options.format == DXGI_FORMAT_R16G16B16A16_FLOAT
            ? DirectXPixelFormat::R16G16B16A16Float : DirectXPixelFormat::B8G8R8A8UIntNormalized;
        pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
            device.winrtDevice, format, options.bufferCount, size);
        session_ = pool_.CreateCaptureSession(item_);
        token_ = pool_.FrameArrived([handler = std::move(handler)](
            const Direct3D11CaptureFramePool& sender, const auto&) noexcept {
            // The handler catches all failures while its callback guard is held.
            // No pool read occurs before it wins the shared completion gate.
            try {
                handler([&sender] {
                    auto frame = sender.TryGetNextFrame();
                    if (!frame) { return IncomingCaptureFrame{}; }
                    auto lifetime = std::make_shared<WgcFrameLease>(std::move(frame));
                    const auto content = lifetime->frame.ContentSize();
                    IncomingCaptureFrame result;
                    result.texture = textureFromSurface(lifetime->frame.Surface());
                    result.contentWidth = content.Width;
                    result.contentHeight = content.Height;
                    const auto timestamp = lifetime->frame.SystemRelativeTime();
                    result.systemRelativeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp);
                    result.lifetime = std::move(lifetime);
                    return result;
                });
            } catch (...) {}  // Last-resort ABI boundary guard (e.g. allocation).
        });
        subscribed_ = true;
        session_.StartCapture();
    }

    void revoke() noexcept override {
        if (subscribed_ && pool_) {
            try { pool_.FrameArrived(token_); } catch (...) {}
        }
        subscribed_ = false;
    }
    void close() noexcept override {
        if (session_) { try { session_.Close(); } catch (...) {} }
        session_ = nullptr;
        if (pool_) { try { pool_.Close(); } catch (...) {} }
        pool_ = nullptr;
        item_ = nullptr;
    }
private:
    GraphicsCaptureItem item_{nullptr};
    Direct3D11CaptureFramePool pool_{nullptr};
    GraphicsCaptureSession session_{nullptr};
    winrt::event_token token_{};
    bool subscribed_{};
};
}  // namespace

struct MonitorCaptureService::Operation final {
    Operation(MonitorCaptureRequest input, CaptureCompletion completion)
        : request(std::move(input)), state(std::move(completion)) {}

    void close() noexcept {
        if (closed) { return; }
        closed = true;
        state.preventCallbacks();
        if (source) { source->revoke(); }
        state.waitForCallbacks();
        if (source) { source->close(); }
        if (timer) { timer->stop(); timer.reset(); }
        source.reset();
    }

    MonitorCaptureRequest request;
    OneShotCaptureState state;
    CapturePoolOptions options;
    std::uint64_t deviceGeneration{};
    std::unique_ptr<IMonitorFrameSource> source;
    std::unique_ptr<QTimer> timer;
    bool closed{};  // Qt main thread only.
};

MonitorCaptureService::MonitorCaptureService(D3d11DeviceManager& manager, QObject* parent)
    : MonitorCaptureService(manager, [] { return std::make_unique<WgcMonitorFrameSource>(); }, parent) {}

bool MonitorCaptureService::isSupported() noexcept {
    try { return GraphicsCaptureSession::IsSupported(); }
    catch (...) { return false; }
}

MonitorCaptureService::MonitorCaptureService(D3d11DeviceManager& manager,
                                           CaptureSourceFactory factory, QObject* parent)
    : QObject(parent), manager_(manager), factory_(std::move(factory)) {
    Q_ASSERT(QCoreApplication::instance());
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
}

MonitorCaptureService::~MonitorCaptureService() { cancel(); }

void MonitorCaptureService::captureOnce(MonitorCaptureRequest request, CaptureCompletion completion) {
    Q_ASSERT(QThread::currentThread() == thread());
    auto operation = std::make_shared<Operation>(std::move(request), std::move(completion));
    if (active_) {
        operation->state.fail({CaptureErrorCode::Cancelled, {}});
        rejectedCompletions_.push_back(operation);
        // Rejected work has no resources; its callback still uses the Qt boundary.
        queueFinalization(operation);
        return;
    }
    active_ = operation;
    try {
        const auto& monitor = operation->request.monitor;
        const auto width = platform::width(monitor.desktopRect);
        const auto height = platform::height(monitor.desktopRect);
        if (!monitor.nativeHandle || !platform::isValid(monitor.desktopRect) ||
            width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max()) {
            winrt::throw_hresult(E_INVALIDARG);
        }
        operation->source = factory_();
        if (!operation->source) { winrt::throw_hresult(E_FAIL); }
        if (!operation->source->isSupported()) {
            operation->state.fail({CaptureErrorCode::Unsupported, {}});
            queueFinalization(operation);
            return;
        }
        const auto device = manager_.current();
        if (!device) {
            const auto error = manager_.lastError();
            operation->state.fail({CaptureErrorCode::DeviceCreationFailed,
                {static_cast<int>(error ? error->nativeCode : E_FAIL), std::system_category()}});
            queueFinalization(operation);
            return;
        }
        operation->deviceGeneration = device->generation;
        auto& options = operation->options;
        options.size = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        options.format = monitor.hdrEnabled ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
        options.timeout = operation->request.timeout;
        if (options.timeout.count() <= 0 || options.timeout.count() > std::numeric_limits<int>::max()) {
            operation->state.fail({CaptureErrorCode::Internal, {}});
            queueFinalization(operation);
            return;
        }
        operation->timer = std::make_unique<QTimer>(this);
        operation->timer->setSingleShot(true);
        operation->timer->setTimerType(Qt::PreciseTimer);
        const std::weak_ptr<Operation> weak = operation;
        QObject::connect(operation->timer.get(), &QTimer::timeout, this, [this, weak] {
            if (const auto current = weak.lock(); current && current->state.timeout()) {
                queueFinalization(current);
            }
        });
        operation->timer->start(static_cast<int>(options.timeout.count()));
        operation->source->start(monitor.nativeHandle, *device, options,
            [this, weak](const FrameReader& reader) noexcept {
                const auto current = weak.lock();
                if (!current || !current->state.enterCallback()) { return; }
                const auto leave = wil::scope_exit([&] { current->state.leaveCallback(); });
                if (!current->state.tryBeginFrame()) { return; }
                try {
                    if (current->state.finishFrame(readAndCopy(*current, reader))) {
                        queueFinalization(current);
                    }
                } catch (const winrt::hresult_error& error) {
                    if (current->state.finishFrame(nativeError(error.code()))) { queueFinalization(current); }
                } catch (...) {
                    if (current->state.finishFrame({CaptureError{CaptureErrorCode::Internal, {}}})) {
                        queueFinalization(current);
                    }
                }
            });
    } catch (const winrt::hresult_error& error) {
        if (operation->state.fail(nativeError(error.code()))) { queueFinalization(operation); }
    } catch (...) {
        if (operation->state.fail({CaptureErrorCode::Internal, {}})) { queueFinalization(operation); }
    }
}

CaptureResult MonitorCaptureService::readAndCopy(Operation& operation, const FrameReader& readFrame) {
    const auto frame = readFrame();
    if (!frame.texture || frame.contentWidth <= 0 || frame.contentHeight <= 0) {
        return CaptureError{CaptureErrorCode::EmptyFrame, {}};
    }
    D3D11_TEXTURE2D_DESC desc{};
    frame.texture->GetDesc(&desc);
    const auto& options = operation.options;
    if (static_cast<std::uint32_t>(frame.contentWidth) != options.size.width ||
        static_cast<std::uint32_t>(frame.contentHeight) != options.size.height ||
        desc.Width != options.size.width || desc.Height != options.size.height ||
        desc.Format != options.format) {
        return CaptureError{CaptureErrorCode::DisplayChanged, {}};
    }
    if (manager_.generation() != operation.deviceGeneration) {
        return CaptureError{CaptureErrorCode::DeviceLost, {}};
    }
    TextureCopy copy(manager_);
    auto copied = copy.copyOwned(*frame.texture);
    if (manager_.generation() != operation.deviceGeneration) {
        return CaptureError{CaptureErrorCode::DeviceLost, {}};
    }
    if (const auto* error = std::get_if<D3dError>(&copied)) {
        auto result = nativeError(error->nativeCode);
        if (error->code == D3dErrorCode::DeviceLost) { result.code = CaptureErrorCode::DeviceLost; }
        return result;
    }
    CaptureFrame result;
    result.texture = std::get<winrt::com_ptr<ID3D11Texture2D>>(std::move(copied));
    result.size = options.size;
    result.pixelFormat = operation.request.monitor.hdrEnabled
        ? CapturePixelFormat::Rgba16Float : CapturePixelFormat::Bgra8Unorm;
    result.systemRelativeTime = frame.systemRelativeTime;
    result.sourceMonitor = operation.request.monitor.id;
    result.displayGeneration = operation.request.monitor.catalogGeneration;
    result.deviceGeneration = operation.deviceGeneration;
    return result;
}

void MonitorCaptureService::queueFinalization(const std::shared_ptr<Operation>& operation) {
    QMetaObject::invokeMethod(this, [this, operation] { finalize(operation); }, Qt::QueuedConnection);
}

void MonitorCaptureService::finalize(const std::shared_ptr<Operation>& operation) noexcept {
    operation->close();
    if (active_ == operation) { active_.reset(); }
    std::erase(rejectedCompletions_, operation);
    operation->state.completeAfterCleanup();  // May destroy this; do not access members afterward.
}

void MonitorCaptureService::cancel() noexcept {
    Q_ASSERT(QThread::currentThread() == thread());
    if (auto operation = std::exchange(active_, {})) {
        operation->state.cancel();
        operation->close();
    }
    for (const auto& operation : rejectedCompletions_) {
        operation->state.cancel();
        operation->close();
    }
    rejectedCompletions_.clear();
}

}  // namespace lc::capture::windows
