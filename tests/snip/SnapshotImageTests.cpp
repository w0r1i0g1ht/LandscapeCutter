#include "snip/SnapshotImage.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>

using namespace lc::snip;
TEST_CASE("snip readback respects padded rows and owns pixels") {
    lc::graphics::d3d11::TextureReadback raw{
        {1, 2}, DXGI_FORMAT_B8G8R8A8_UNORM, 8, std::vector<std::byte>(16)};
    const unsigned char bytes[] = {0, 0, 255, 255, 9, 9, 9, 9, 0, 255, 0, 255, 9, 9, 9, 9};
    std::memcpy(raw.bytes.data(), bytes, 16);
    const auto image = imageFromReadback(raw);
    raw.bytes.clear();
    REQUIRE(image.size() == QSize(1, 2));
    CHECK(image.pixelColor(0, 0) == QColor(Qt::red));
    CHECK(image.pixelColor(0, 1) == QColor(Qt::green));
    CHECK(image.devicePixelRatio() == 1.0);
}
TEST_CASE("snip malformed readback is rejected") {
    lc::graphics::d3d11::TextureReadback raw{
        {2, 2}, DXGI_FORMAT_B8G8R8A8_UNORM, 4, std::vector<std::byte>(8)};
    CHECK(imageFromReadback(raw).isNull());
    raw.rowPitch = 8;
    CHECK(imageFromReadback(raw).isNull());
}
TEST_CASE("snip float readback converts linear color and sanitizes nonfinite values") {
    lc::graphics::d3d11::TextureReadback raw{
        {2, 1}, DXGI_FORMAT_R16G16B16A16_FLOAT, 16, std::vector<std::byte>(16)};
    const unsigned short values[] = {0x3c00, 0x0000, 0x0000, 0x3c00,
                                     0x3800, 0x7e00, 0x7c00, 0x3c00};
    std::memcpy(raw.bytes.data(), values, 16);
    const auto image = imageFromReadback(raw);
    REQUIRE_FALSE(image.isNull());
    CHECK(image.pixelColor(0, 0) == QColor(Qt::red));
    CHECK(image.pixelColor(1, 0) == QColor(188, 0, 255));
}
TEST_CASE("snip composition preserves physical pixels across negative monitors and gaps") {
    QImage red(2, 2, QImage::Format_RGB32);
    red.fill(Qt::red);
    red.setDevicePixelRatio(2);
    QImage blue(2, 2, QImage::Format_RGB32);
    blue.fill(Qt::blue);
    const std::vector<FrozenMonitor> screens = {{{-3, 0, 2, 2}, red}, {{0, 0, 2, 2}, blue}};
    const auto image = composeSelection(screens, {-2, 0, 3, 2});
    REQUIRE(image.size() == QSize(3, 2));
    CHECK(image.pixelColor(0, 0) == QColor(Qt::red));
    CHECK(image.pixelColor(1, 0) == QColor(Qt::black));
    CHECK(image.pixelColor(2, 0) == QColor(Qt::blue));
    CHECK(composeSelection(screens, {-1, 0, 1, 2}).isNull());
    CHECK(composeSelection(screens, {0, 0, 0, 2}).isNull());
}
TEST_CASE("snip atomic export roundtrips and failed encoding preserves destination") {
    int argc = 1;
    char name[] = "image-test";
    char* argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = dir.filePath("capture.png");
    QImage red(16, 16, QImage::Format_RGB32);
    for (int y = 0; y < red.height(); ++y)
        for (int x = 0; x < red.width(); ++x)
            red.setPixelColor(x, y, {x * 15, y * 15, (x + y) * 7});
    REQUIRE(saveImage(red, path, "png").isEmpty());
    CHECK(QImage(path).convertToFormat(QImage::Format_RGB32) == red);
    REQUIRE_FALSE(saveImage(red, path, "invalid-format").isEmpty());
    CHECK(QImage(path).convertToFormat(QImage::Format_RGB32) == red);
    const auto jpeg = dir.filePath("capture.jpg");
    REQUIRE(saveImage(red, jpeg, "jpeg").isEmpty());
    REQUIRE(QImage(jpeg).size() == red.size());
    const auto decoded = QImage(jpeg).convertToFormat(QImage::Format_RGB32);
    double absoluteError{};
    for (int y = 0; y < red.height(); ++y)
        for (int x = 0; x < red.width(); ++x) {
            const auto expected = red.pixelColor(x, y);
            const auto actual = decoded.pixelColor(x, y);
            absoluteError += std::abs(expected.red() - actual.red()) +
                             std::abs(expected.green() - actual.green()) +
                             std::abs(expected.blue() - actual.blue());
        }
    CHECK(absoluteError / (red.width() * red.height() * 3) <= 12.0);
    CHECK_FALSE(saveImage(red, dir.filePath("missing/file.png"), "png").isEmpty());
}

TEST_CASE("snip cancelled export leaves an existing destination untouched") {
    int argc = 1;
    char name[] = "cancel-image-test";
    char* argv[] = {name, nullptr};
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = dir.filePath("capture.png");
    QImage red(4, 4, QImage::Format_RGB32);
    red.fill(Qt::red);
    REQUIRE(saveImage(red, path, "png").isEmpty());
    QImage blue(4, 4, QImage::Format_RGB32);
    blue.fill(Qt::blue);
    std::atomic_bool cancelled = true;

    CHECK_FALSE(saveImage(blue, path, "png", &cancelled).isEmpty());
    CHECK(QImage(path).pixelColor(0, 0) == QColor(Qt::red));
}
