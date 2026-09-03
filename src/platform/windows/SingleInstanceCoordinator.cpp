#include "platform/windows/SingleInstanceCoordinator.hpp"

#include <QCoreApplication>
#include <QThread>
#include <QWinEventNotifier>

#include <string>

namespace lc::platform::windows {
namespace {

constexpr wchar_t kProductNamespace[] =
    L"Local\\LandscapeCutter.4F4E6D0D-8C33-4B79-984A-3E44F6A62D11";

std::wstring buildObjectName(const std::wstring& namespaceSuffix, const wchar_t* objectSuffix) {
    return std::wstring{kProductNamespace} + namespaceSuffix + objectSuffix;
}

}  // namespace

SingleInstanceCoordinator::SingleInstanceCoordinator(std::wstring namespaceSuffix, QObject* parent)
    : QObject(parent), namespaceSuffix_(std::move(namespaceSuffix)) {}

SingleInstanceCoordinator::~SingleInstanceCoordinator() {
    Q_ASSERT(QThread::currentThread() == thread());

    notifier_.reset();
    instanceMutex_.reset();
    activationEvent_.reset();
}

InstanceAcquireResult SingleInstanceCoordinator::acquire() {
    if (activationEvent_.is_valid() && instanceMutex_.is_valid()) {
        return {role_, ERROR_SUCCESS};
    }

    const std::wstring activationEventName = buildObjectName(namespaceSuffix_, L".Activate");
    activationEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, activationEventName.c_str()));
    if (!activationEvent_.is_valid()) {
        const unsigned long nativeError = GetLastError();
        return {InstanceRole::Failed, nativeError};
    }

    const std::wstring instanceMutexName = buildObjectName(namespaceSuffix_, L".Instance");
    instanceMutex_.reset(CreateMutexW(nullptr, FALSE, instanceMutexName.c_str()));
    if (!instanceMutex_.is_valid()) {
        const unsigned long nativeError = GetLastError();
        activationEvent_.reset();
        return {InstanceRole::Failed, nativeError};
    }

    role_ = GetLastError() == ERROR_ALREADY_EXISTS ? InstanceRole::Secondary : InstanceRole::Primary;
    return {role_, ERROR_SUCCESS};
}

bool SingleInstanceCoordinator::signalPrimary() noexcept {
    return role_ == InstanceRole::Secondary && activationEvent_.is_valid() &&
           SetEvent(activationEvent_.get()) != FALSE;
}

bool SingleInstanceCoordinator::beginListening() {
    Q_ASSERT(QThread::currentThread() == thread());

    if (role_ != InstanceRole::Primary || !activationEvent_.is_valid() ||
        QCoreApplication::instance() == nullptr) {
        return false;
    }

    if (notifier_ != nullptr) {
        return true;
    }

    notifier_ = std::make_unique<QWinEventNotifier>(activationEvent_.get());
    connect(notifier_.get(), &QWinEventNotifier::activated, this,
            [this](Qt::HANDLE) { emit activationRequested(); });
    return true;
}

InstanceRole SingleInstanceCoordinator::role() const noexcept {
    return role_;
}

}  // namespace lc::platform::windows
