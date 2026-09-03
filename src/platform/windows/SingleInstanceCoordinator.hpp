#pragma once

#include <Windows.h>

#include <QObject>

#include <wil/resource.h>

#include <memory>
#include <string>

class QWinEventNotifier;

namespace lc::platform::windows {

enum class InstanceRole { Primary, Secondary, Failed };

struct InstanceAcquireResult final {
    InstanceRole role;
    unsigned long nativeError;
};

class SingleInstanceCoordinator final : public QObject {
    Q_OBJECT

public:
    explicit SingleInstanceCoordinator(std::wstring namespaceSuffix = {}, QObject* parent = nullptr);
    ~SingleInstanceCoordinator() override;

    InstanceAcquireResult acquire();
    bool signalPrimary() noexcept;
    bool beginListening();
    InstanceRole role() const noexcept;

signals:
    void activationRequested();

private:
    std::wstring namespaceSuffix_;
    wil::unique_handle activationEvent_;
    wil::unique_handle instanceMutex_;
    std::unique_ptr<QWinEventNotifier> notifier_;
    InstanceRole role_ = InstanceRole::Failed;
};

}  // namespace lc::platform::windows
