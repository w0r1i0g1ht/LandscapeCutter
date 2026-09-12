#include "snip/SnapshotImage.hpp"
#include <QImageWriter>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace lc::snip {
namespace {
// Bound single allocations to 512 MiB; reject malformed dimensions before Qt arithmetic.
bool validSize(qint64 w, qint64 h) {
    return w > 0 && h > 0 && w <= 32768 && h <= 32768 && w * h <= 134217728;
}
float half(unsigned short bits) {
    const int exponent = (bits >> 10) & 31;
    const int fraction = bits & 1023;
    float value = exponent == 0    ? std::ldexp(static_cast<float>(fraction), -24)
                  : exponent == 31 ? (fraction ? std::numeric_limits<float>::quiet_NaN()
                                               : std::numeric_limits<float>::infinity())
                                   : std::ldexp(static_cast<float>(1024 + fraction), exponent - 25);
    return bits & 0x8000 ? -value : value;
}
int srgb(float linear) {
    if (std::isnan(linear) || linear <= 0)
        return 0;
    if (linear >= 1)
        return 255;
    const float encoded =
        linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    return static_cast<int>(std::lround(encoded * 255));
}
} // namespace
QImage imageFromReadback(const graphics::d3d11::TextureReadback& raw) {
    if (!validSize(raw.size.width, raw.size.height))
        return {};
    const bool hdr = raw.format == DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (!hdr && raw.format != DXGI_FORMAT_B8G8R8A8_UNORM)
        return {};
    const std::size_t rowBytes = static_cast<std::size_t>(raw.size.width) * (hdr ? 8 : 4);
    if (raw.rowPitch < rowBytes || raw.rowPitch > raw.bytes.size() / raw.size.height)
        return {};
    QImage image(static_cast<int>(raw.size.width), static_cast<int>(raw.size.height),
                 QImage::Format_RGB32);
    if (image.isNull())
        return {};
    for (int y = 0; y < image.height(); ++y) {
        auto* dst = reinterpret_cast<QRgb*>(image.scanLine(y));
        const auto* src = raw.bytes.data() + static_cast<std::size_t>(y) * raw.rowPitch;
        for (int x = 0; x < image.width(); ++x) {
            if (hdr) {
                unsigned short rgba[4];
                std::memcpy(rgba, src + static_cast<std::size_t>(x) * 8, 8);
                dst[x] = qRgb(srgb(half(rgba[0])), srgb(half(rgba[1])), srgb(half(rgba[2])));
            } else {
                const auto* bgra =
                    reinterpret_cast<const unsigned char*>(src) + static_cast<std::size_t>(x) * 4;
                dst[x] = qRgb(bgra[2], bgra[1], bgra[0]);
            }
        }
    }
    return image;
}
QImage composeSelection(const std::vector<FrozenMonitor>& monitors, QRect selection) {
    if (!validSize(selection.width(), selection.height()))
        return {};
    bool covered = false;
    for (const auto& monitor : monitors) {
        if (monitor.image.size() != monitor.geometry.size() || monitor.image.isNull())
            return {};
        covered |= monitor.geometry.intersects(selection);
    }
    if (!covered)
        return {};
    QImage result(selection.size(), QImage::Format_RGB32);
    if (result.isNull())
        return {};
    result.fill(Qt::black);
    for (const auto& monitor : monitors) {
        const QRect overlap = monitor.geometry.intersected(selection);
        if (overlap.isEmpty())
            continue;
        const auto image = monitor.image.convertToFormat(QImage::Format_RGB32);
        if (image.isNull())
            return {};
        for (int y = 0; y < overlap.height(); ++y) {
            std::memcpy(result.scanLine(overlap.y() - selection.y() + y) +
                            (overlap.x() - selection.x()) * 4,
                        image.constScanLine(overlap.y() - monitor.geometry.y() + y) +
                            (overlap.x() - monitor.geometry.x()) * 4,
                        static_cast<std::size_t>(overlap.width()) * 4);
        }
    }
    return result;
}
QString saveImage(const QImage& image, const QString& path, const QByteArray& format,
                  const std::atomic_bool* cancelled, std::mutex* finalizationMutex) {
    if (image.isNull())
        return QStringLiteral("没有可保存的图像。");
    if (cancelled != nullptr && cancelled->load(std::memory_order_acquire)) {
        return QStringLiteral("保存已取消。");
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return file.errorString();
    QImageWriter writer(&file, format);
    if (format.compare("jpeg", Qt::CaseInsensitive) == 0 ||
        format.compare("jpg", Qt::CaseInsensitive) == 0)
        writer.setQuality(90);
    if (!writer.write(image)) {
        file.cancelWriting();
        return writer.errorString();
    }
    std::unique_lock<std::mutex> finalizationLock;
    if (finalizationMutex != nullptr)
        finalizationLock = std::unique_lock<std::mutex>(*finalizationMutex);
    if (cancelled != nullptr && cancelled->load(std::memory_order_acquire)) {
        file.cancelWriting();
        return QStringLiteral("保存已取消。");
    }
    if (!file.commit())
        return file.errorString();
    return {};
}
} // namespace lc::snip
