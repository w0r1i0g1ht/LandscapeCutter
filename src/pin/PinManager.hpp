#pragma once

#include "pin/PinTypes.hpp"
#include "pin/PinWindow.hpp"

#include <QObject>
#include <QPointer>

#include <cstddef>
#include <functional>
#include <unordered_map>

namespace lc::pin {
using AvailablePinGeometries = std::function<QList<QRect>()>;

class PinManager final : public QObject {
    Q_OBJECT

  public:
    explicit PinManager(CreatePinWindow factory = {}, ChoosePinSavePath savePathChooser = {},
                        AvailablePinGeometries availableGeometries = {}, QObject* parent = nullptr);
    ~PinManager() override;

    PinCreateResult create(std::unique_ptr<annotation::AnnotationDocument> document,
                           QPoint preferredTopLeft);
    void close(PinId id);
    void closeAll();
    void recoverVisibility(const QList<QRect>& availableGeometries);
    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] PinWindow* window(PinId id) const noexcept;

  signals:
    void countChanged(std::size_t count);
    void errorOccurred(QString message);

  private:
    void remove(PinId id);
    void emitCountChanged();

    CreatePinWindow factory_;
    AvailablePinGeometries availableGeometries_;
    std::unordered_map<PinId, QPointer<PinWindow>> windows_;
    PinId nextId_{1};
};
} // namespace lc::pin
