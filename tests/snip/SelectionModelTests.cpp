#include "snip/SelectionModel.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <tuple>
#include <utility>

namespace {
using lc::snip::SelectionHit;
using lc::snip::SelectionModel;

SelectionModel selectionWithRect(const QRect& bounds, const QPoint& start, const QPoint& end) {
    SelectionModel model;
    model.setBounds(bounds);
    model.press(start, 2);
    model.move(end);
    model.release();
    return model;
}

TEST_CASE("reverse creation normalizes to a physical half-open rectangle") {
    auto model = selectionWithRect(QRect{-100, -100, 300, 300}, QPoint{10, 10}, QPoint{-10, -5});

    CHECK(model.rect() == QRect{-10, -5, 20, 15});
}

TEST_CASE("zero-length creation remains invalid") {
    auto model = selectionWithRect(QRect{0, 0, 100, 100}, QPoint{10, 10}, QPoint{10, 10});

    CHECK_FALSE(model.rect().isValid());
    CHECK(model.hitTest(QPoint{10, 10}, 2) == SelectionHit::Create);
}

TEST_CASE("selection movement preserves size and clamps at physical bounds") {
    auto model = selectionWithRect(QRect{0, 0, 100, 100}, QPoint{20, 20}, QPoint{60, 50});
    REQUIRE(model.rect() == QRect{20, 20, 40, 30});

    model.press(QPoint{30, 30}, 2);
    model.move(QPoint{-100, -100});
    model.release();
    CHECK(model.rect() == QRect{0, 0, 40, 30});

    model.press(QPoint{10, 10}, 2);
    model.move(QPoint{500, 500});
    model.release();
    CHECK(model.rect() == QRect{60, 70, 40, 30});
}

TEST_CASE("hit testing identifies all eight physical resize handles") {
    const auto model = selectionWithRect(QRect{0, 0, 100, 100}, QPoint{20, 20}, QPoint{60, 50});

    const std::array cases{
        std::pair{QPoint{20, 20}, SelectionHit::TopLeft},
        std::pair{QPoint{60, 20}, SelectionHit::TopRight},
        std::pair{QPoint{20, 50}, SelectionHit::BottomLeft},
        std::pair{QPoint{60, 50}, SelectionHit::BottomRight},
        std::pair{QPoint{20, 35}, SelectionHit::Left},
        std::pair{QPoint{60, 35}, SelectionHit::Right},
        std::pair{QPoint{40, 20}, SelectionHit::Top},
        std::pair{QPoint{40, 50}, SelectionHit::Bottom},
    };

    for (const auto& [point, expected] : cases) {
        CHECK(model.hitTest(point, 0) == expected);
    }
    CHECK(model.hitTest(QPoint{40, 35}, 0) == SelectionHit::Move);
    CHECK(model.hitTest(QPoint{5, 5}, 0) == SelectionHit::Create);
}

TEST_CASE("every handle resizes only its corresponding physical edges") {
    const QRect bounds{0, 0, 100, 100};
    const QPoint start{20, 20};
    const QPoint end{60, 50};
    const std::array cases{
        std::tuple{QPoint{20, 20}, QPoint{10, 10}, QRect{10, 10, 50, 40}},
        std::tuple{QPoint{60, 20}, QPoint{70, 10}, QRect{20, 10, 50, 40}},
        std::tuple{QPoint{20, 50}, QPoint{10, 60}, QRect{10, 20, 50, 40}},
        std::tuple{QPoint{60, 50}, QPoint{70, 60}, QRect{20, 20, 50, 40}},
        std::tuple{QPoint{20, 35}, QPoint{10, 35}, QRect{10, 20, 50, 30}},
        std::tuple{QPoint{60, 35}, QPoint{70, 35}, QRect{20, 20, 50, 30}},
        std::tuple{QPoint{40, 20}, QPoint{40, 10}, QRect{20, 10, 40, 40}},
        std::tuple{QPoint{40, 50}, QPoint{40, 60}, QRect{20, 20, 40, 40}},
    };

    for (const auto& [handle, destination, expected] : cases) {
        auto model = selectionWithRect(bounds, start, end);
        model.press(handle, 0);
        model.move(destination);
        model.release();
        CHECK(model.rect() == expected);
    }
}

TEST_CASE("resizing stops at one physical pixel") {
    auto model = selectionWithRect(QRect{0, 0, 100, 100}, QPoint{20, 20}, QPoint{60, 50});

    model.press(QPoint{20, 35}, 0);
    model.move(QPoint{100, 35});
    model.release();
    CHECK(model.rect() == QRect{59, 20, 1, 30});

    model.press(QPoint{59, 35}, 0);
    model.move(QPoint{-100, 35});
    model.release();
    CHECK(model.rect() == QRect{0, 20, 60, 30});
}

TEST_CASE("bounds and drag arithmetic do not overflow at integer limits") {
    constexpr int max = std::numeric_limits<int>::max();
    auto model = selectionWithRect(QRect{max - 100, max - 100, 100, 100},
                                   QPoint{max - 80, max - 80}, QPoint{max - 40, max - 50});
    REQUIRE(model.rect() == QRect{max - 80, max - 80, 40, 30});

    model.press(QPoint{max - 70, max - 70}, 0);
    model.move(QPoint{max, max});
    model.release();

    CHECK(model.rect() == QRect{max - 40, max - 30, 40, 30});
}
} // namespace
