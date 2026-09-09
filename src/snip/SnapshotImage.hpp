#pragma once
#include "graphics/d3d11/TextureCopy.hpp"
#include <QImage>
#include <QRect>
#include <QString>
#include <atomic>
#include <vector>

namespace lc::snip {
struct FrozenMonitor {
    QRect geometry;
    QImage image;
    HMONITOR nativeHandle{};
};
QImage imageFromReadback(const graphics::d3d11::TextureReadback& raw);
QImage composeSelection(const std::vector<FrozenMonitor>& monitors, QRect selection);
QString saveImage(const QImage& image, const QString& path, const QByteArray& format,
                  const std::atomic_bool* cancelled = nullptr);
} // namespace lc::snip
