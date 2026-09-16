#pragma once

#include <QList>
#include <QRect>
#include <QSize>

namespace lc::pin {
struct PinScreenGeometry final {
    QRect physicalGeometry;
    QRect logicalGeometry;
    QRect availableLogicalGeometry;
};

[[nodiscard]] QRect initialPinWindowRect(QRect physicalSelection, QSize documentPhysicalSize,
                                         const QList<PinScreenGeometry>& screens) noexcept;

class PinGeometryModel final {
  public:
    PinGeometryModel(QSize documentSize, QRect windowRect);

    void moveTo(QPoint topLeft) noexcept;
    void zoomAt(QPoint desktopAnchor, int angleDeltaY) noexcept;
    void adjustOpacity(int angleDeltaY) noexcept;
    void resetSize() noexcept;
    [[nodiscard]] bool ensureOperable(const QList<QRect>& availableGeometries) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] QRect windowRect() const noexcept;
    [[nodiscard]] qreal opacity() const noexcept;

  private:
    QSize documentSize_;
    QSize originalWindowSize_;
    QRect windowRect_;
    qreal opacity_{1.0};
    bool valid_{};
};
} // namespace lc::pin
