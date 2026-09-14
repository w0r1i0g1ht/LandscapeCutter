#include "pin/PinGeometryModel.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace {
using Catch::Approx;
using lc::pin::PinGeometryModel;

TEST_CASE("pin geometry zoom keeps the cursor anchored") {
    PinGeometryModel model({400, 200}, {100, 100, 400, 200});

    model.zoomAt({300, 200}, 120);

    CHECK(model.windowRect() == QRect(80, 90, 440, 220));
}

TEST_CASE("pin geometry zoom preserves ratio and minimum edge") {
    PinGeometryModel model({400, 200}, {100, 100, 400, 200});

    model.zoomAt({300, 200}, -12000);

    CHECK(model.windowRect().size() == QSize(64, 32));
    CHECK(model.windowRect().center() == QPoint(299, 199));
}

TEST_CASE("pin geometry opacity uses five-percent steps and clamps") {
    PinGeometryModel model({40, 20}, {0, 0, 40, 20});

    model.adjustOpacity(-10000);
    CHECK(model.opacity() == Approx(0.10));
    model.adjustOpacity(10000);
    CHECK(model.opacity() == Approx(1.00));
    model.adjustOpacity(-120);
    CHECK(model.opacity() == Approx(0.95));
}

TEST_CASE("pin geometry restores original pixels without moving the top-left") {
    PinGeometryModel model({400, 200}, {30, 40, 800, 400});

    model.resetSize();

    CHECK(model.windowRect() == QRect(30, 40, 400, 200));
}

TEST_CASE("pin recovery moves only a completely hidden window") {
    const QList<QRect> screens{{0, 0, 1920, 1040}};
    PinGeometryModel hidden({400, 200}, {-900, 20, 400, 200});
    PinGeometryModel visible({400, 200}, {-368, 20, 400, 200});

    CHECK(hidden.ensureOperable(screens));
    CHECK(hidden.windowRect() == QRect(0, 20, 400, 200));
    CHECK_FALSE(visible.ensureOperable(screens));
    CHECK(visible.windowRect() == QRect(-368, 20, 400, 200));
}

TEST_CASE("pin geometry rejects invalid inputs and ignores empty screen lists") {
    PinGeometryModel invalid({}, {});
    CHECK_FALSE(invalid.valid());
    invalid.zoomAt({}, 120);
    invalid.adjustOpacity(-120);
    invalid.resetSize();
    CHECK_FALSE(invalid.ensureOperable({}));
    CHECK(invalid.windowRect().isEmpty());

    PinGeometryModel valid({40, 20}, {10, 20, 40, 20});
    CHECK(valid.valid());
    CHECK_FALSE(valid.ensureOperable({}));
    CHECK(valid.windowRect() == QRect(10, 20, 40, 20));
}

TEST_CASE("pin geometry keeps extreme zoom representable") {
    PinGeometryModel model({400, 200}, {100, 100, 400, 200});

    model.zoomAt({300, 200}, std::numeric_limits<int>::max());

    CHECK(model.windowRect() == QRect(100, 100, 400, 200));
}
} // namespace
