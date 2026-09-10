#include "annotation/AnnotationPreparation.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {
QImage testImage() {
    QImage image(4, 3, QImage::Format_RGB32);
    image.fill(Qt::red);
    return image;
}
} // namespace

TEST_CASE("annotation preparation composes the frozen selection") {
    const std::vector<lc::snip::FrozenMonitor> monitors{{{{0, 0, 4, 3}, testImage()}}};
    QImage prepared;
    lc::snip::prepareAnnotation(monitors, {1, 1, 2, 2},
                                std::make_shared<std::atomic_bool>(false),
                                [&prepared](QImage image) { prepared = std::move(image); });

    REQUIRE_FALSE(prepared.isNull());
    CHECK(prepared.size() == QSize(2, 2));
    CHECK(prepared.devicePixelRatio() == 1.0);
}

TEST_CASE("cancelled annotation preparation does not invoke completion") {
    const std::vector<lc::snip::FrozenMonitor> monitors{{{{0, 0, 4, 3}, testImage()}}};
    auto cancelled = std::make_shared<std::atomic_bool>(true);
    bool completed = false;

    lc::snip::prepareAnnotation(monitors, {0, 0, 2, 2}, cancelled,
                                [&completed](QImage) { completed = true; });

    CHECK_FALSE(completed);
}
