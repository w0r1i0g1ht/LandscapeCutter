#include "snip/SnipOverlay.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QToolButton>

#include <catch2/catch_test_macros.hpp>

namespace {
using lc::snip::FrozenMonitor;
using lc::snip::SelectionModel;
using lc::snip::SnipOverlay;

class ApplicationFixture {
  private:
    int argc{1};
    char applicationName[18]{"snip-overlay-test"};
    char* argv[2]{applicationName, nullptr};

  public:
    ApplicationFixture() : application(argc, argv) {}

    QApplication application;
};

FrozenMonitor frozenMonitor() {
    QImage image{QSize{40, 30}, QImage::Format_RGB32};
    image.fill(Qt::red);
    return {{-40, 10, 40, 30}, image};
}

SelectionModel selectionFor(const QRect& bounds) {
    SelectionModel model;
    model.setBounds(bounds);
    model.press({-30, 15}, 2);
    model.move({-10, 35});
    model.release();
    return model;
}

TEST_CASE("overlay presents the frozen monitor and shared selection controls") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};

    CHECK(overlay.windowFlags().testFlag(Qt::FramelessWindowHint));
    CHECK(overlay.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    CHECK(overlay.size() == QSize{40, 30});

    auto* copy = overlay.findChild<QToolButton*>("copyButton");
    auto* save = overlay.findChild<QToolButton*>("saveButton");
    auto* cancel = overlay.findChild<QToolButton*>("cancelButton");
    REQUIRE(copy != nullptr);
    REQUIRE(save != nullptr);
    REQUIRE(cancel != nullptr);
    CHECK(copy->isEnabled());
    CHECK(save->isEnabled());
    CHECK(cancel->isEnabled());

    overlay.setBusy(true);
    CHECK_FALSE(copy->isEnabled());
    CHECK_FALSE(save->isEnabled());
    CHECK(cancel->isEnabled());
    overlay.setBusy(false);
    CHECK(copy->isEnabled());
    CHECK(save->isEnabled());

    overlay.refresh();
}

TEST_CASE("overlay keyboard shortcuts signal shared selection actions") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};
    int copies{};
    int saves{};
    int cancellations{};
    QObject::connect(&overlay, &SnipOverlay::copyRequested, [&copies] { ++copies; });
    QObject::connect(&overlay, &SnipOverlay::saveRequested, [&saves] { ++saves; });
    QObject::connect(&overlay, &SnipOverlay::cancelRequested,
                     [&cancellations] { ++cancellations; });

    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &enter);
    QKeyEvent copy{QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier};
    QApplication::sendEvent(&overlay, &copy);
    QKeyEvent save{QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier};
    QApplication::sendEvent(&overlay, &save);
    QKeyEvent escape{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &escape);

    CHECK(copies == 2);
    CHECK(saves == 1);
    CHECK(cancellations == 1);
}

TEST_CASE("overlay shortcuts do not copy or save during an empty selection") {
    ApplicationFixture fixture;
    SelectionModel model;
    model.setBounds({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};
    int copies{};
    int saves{};
    QObject::connect(&overlay, &SnipOverlay::copyRequested, [&copies] { ++copies; });
    QObject::connect(&overlay, &SnipOverlay::saveRequested, [&saves] { ++saves; });

    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier};
    QApplication::sendEvent(&overlay, &enter);
    QKeyEvent save{QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier};
    QApplication::sendEvent(&overlay, &save);

    CHECK(copies == 0);
    CHECK(saves == 0);
}

TEST_CASE("a busy overlay does not change the shared selection through mouse input") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    const auto original = model.rect();
    SnipOverlay overlay{frozenMonitor(), model};
    overlay.setBusy(true);

    QMouseEvent press{QEvent::MouseButtonPress, QPointF{10.0, 10.0}, QPointF{10.0, 10.0},
                      Qt::LeftButton,           Qt::LeftButton,      Qt::NoModifier};
    QMouseEvent move{QEvent::MouseMove, QPointF{30.0, 20.0}, QPointF{30.0, 20.0},
                     Qt::NoButton,      Qt::LeftButton,      Qt::NoModifier};
    QMouseEvent doubleClick{QEvent::MouseButtonDblClick,
                            QPointF{10.0, 10.0},
                            QPointF{10.0, 10.0},
                            Qt::LeftButton,
                            Qt::LeftButton,
                            Qt::NoModifier};
    QApplication::sendEvent(&overlay, &press);
    QApplication::sendEvent(&overlay, &move);
    QApplication::sendEvent(&overlay, &doubleClick);

    CHECK(model.rect() == original);
}

TEST_CASE("an overlay outside the selection does not show a duplicate toolbar") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 80, 30});
    QImage image{QSize{40, 30}, QImage::Format_RGB32};
    image.fill(Qt::blue);
    SnipOverlay overlay{{{0, 10, 40, 30}, image}, model};

    auto* toolbar = overlay.findChild<QWidget*>("snipToolbar");
    REQUIRE(toolbar != nullptr);
    CHECK(toolbar->isHidden());
}

TEST_CASE("closing an overlay requests cancellation") {
    ApplicationFixture fixture;
    auto model = selectionFor({-40, 10, 40, 30});
    SnipOverlay overlay{frozenMonitor(), model};
    int cancellations{};
    QObject::connect(&overlay, &SnipOverlay::cancelRequested,
                     [&cancellations] { ++cancellations; });

    overlay.close();

    CHECK(cancellations == 1);
}
} // namespace
