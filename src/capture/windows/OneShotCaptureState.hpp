#pragma once

#include "capture/CaptureResult.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>

namespace lc::capture::windows {

// A frame claims the same terminal gate used by timeout and cancellation before
// touching the frame pool. Only main-thread finalization may deliver completion.
class OneShotCaptureState final {
public:
    enum class Phase { AwaitingFrame, ProcessingFrame, Finalizing, Completed, Cancelled };

    explicit OneShotCaptureState(CaptureCompletion completion)
        : completion_(std::move(completion)) {}
    ~OneShotCaptureState() { cancel(); }

    Phase phase() const noexcept { return phase_.load(); }

    bool tryBeginFrame() noexcept {
        auto expected = Phase::AwaitingFrame;
        return phase_.compare_exchange_strong(expected, Phase::ProcessingFrame);
    }

    bool finishFrame(CaptureResult result) {
        return finish(Phase::ProcessingFrame, std::move(result));
    }

    bool fail(CaptureError error) { return finish(Phase::AwaitingFrame, error); }
    bool timeout() { return fail({CaptureErrorCode::Timeout, {}}); }

    void cancel() noexcept {
        std::lock_guard lock(mutex_);
        if (phase_ != Phase::Completed) { phase_ = Phase::Cancelled; }
        acceptingCallbacks_ = false;
        completion_ = {};
        result_.reset();
    }

    // Must follow token revocation, callback drain, session/pool close and timer
    // destruction. The service releases its active request before calling this,
    // so completion may start another request or destroy the service.
    void completeAfterCleanup() noexcept {
        CaptureCompletion completion;
        std::optional<CaptureResult> result;
        {
            std::lock_guard lock(mutex_);
            auto expected = Phase::Finalizing;
            if (!phase_.compare_exchange_strong(expected, Phase::Completed)) { return; }
            completion = std::move(completion_);
            result = std::move(result_);
            result_.reset();
        }
        // User callbacks must also not unwind through the Qt event dispatcher.
        if (completion && result) {
            try { completion(std::move(*result)); } catch (...) {}
        }
    }

    bool enterCallback() {
        std::lock_guard lock(mutex_);
        if (!acceptingCallbacks_) { return false; }
        ++activeCallbacks_;
        return true;
    }
    void leaveCallback() noexcept {
        std::lock_guard lock(mutex_);
        --activeCallbacks_;
        callbacksDrained_.notify_all();
    }
    void preventCallbacks() noexcept {
        std::lock_guard lock(mutex_);
        acceptingCallbacks_ = false;
    }
    void waitForCallbacks() noexcept {
        std::unique_lock lock(mutex_);
        callbacksDrained_.wait(lock, [&] { return activeCallbacks_ == 0; });
    }
    std::size_t activeCallbacks() const {
        std::lock_guard lock(mutex_);
        return activeCallbacks_;
    }

private:
    bool finish(Phase from, CaptureResult result) {
        std::lock_guard lock(mutex_);
        if (!phase_.compare_exchange_strong(from, Phase::Finalizing)) { return false; }
        result_.emplace(std::move(result));
        return true;
    }

    std::atomic<Phase> phase_{Phase::AwaitingFrame};
    mutable std::mutex mutex_;
    std::condition_variable callbacksDrained_;
    bool acceptingCallbacks_{true};
    std::size_t activeCallbacks_{};
    CaptureCompletion completion_;
    std::optional<CaptureResult> result_;
};

}  // namespace lc::capture::windows
