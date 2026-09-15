#include "pin/PinManager.hpp"

#include "../annotation/AnnotationTestApplication.hpp"
#include "annotation/AnnotationDocument.hpp"
#include "app/AppController.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QPointer>

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace {
using lc::annotation::AnnotationDocument;
using lc::pin::PinManager;

std::unique_ptr<AnnotationDocument> makeDocument(QSize size = {40, 20}) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);
    return std::make_unique<AnnotationDocument>(image);
}

TEST_CASE("pin manager returns the document when window creation fails") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    PinManager manager([](lc::pin::PinId) -> lc::pin::PinWindow* { return nullptr; });
    auto document = makeDocument();
    auto* original = document.get();

    auto result = manager.create(std::move(document), {10, 10});

    CHECK_FALSE(result.id.has_value());
    CHECK(result.rejectedDocument.get() == original);
    CHECK_FALSE(result.error.isEmpty());
    CHECK(manager.count() == 0);
}

TEST_CASE("pin manager rejects an empty document without allocating a window") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    int factoryCalls{};
    PinManager manager([&factoryCalls](lc::pin::PinId) -> lc::pin::PinWindow* {
        ++factoryCalls;
        return nullptr;
    });
    std::unique_ptr<AnnotationDocument> empty;

    auto result = manager.create(std::move(empty), {});

    CHECK_FALSE(result.id.has_value());
    CHECK(result.rejectedDocument == nullptr);
    CHECK_FALSE(result.error.isEmpty());
    CHECK(factoryCalls == 0);
}

TEST_CASE("pin manager keeps several windows independent and ids increase") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    PinManager manager;
    const auto first = manager.create(makeDocument({40, 20}), {0, 0});
    const auto second = manager.create(makeDocument({60, 30}), {100, 0});
    REQUIRE(first.id.has_value());
    REQUIRE(second.id.has_value());
    CHECK(*first.id != 0);
    CHECK(*second.id > *first.id);
    CHECK(manager.count() == 2);

    manager.close(*first.id);
    CHECK(manager.count() == 1);
    manager.close(*first.id);
    CHECK(manager.count() == 1);
    manager.closeAll();
    CHECK(manager.count() == 0);
    manager.closeAll();
    CHECK(manager.count() == 0);
}

TEST_CASE("pin manager reports count changes and removes a user closed window") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    PinManager manager;
    std::vector<std::size_t> counts;
    QObject::connect(&manager, &PinManager::countChanged,
                     [&counts](const std::size_t count) { counts.push_back(count); });
    const auto result = manager.create(makeDocument(), {});
    REQUIRE(result.id.has_value());
    auto* window = manager.window(*result.id);
    REQUIRE(window != nullptr);
    window->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    CHECK(manager.count() == 0);
    CHECK(counts == std::vector<std::size_t>{1, 0});
}

TEST_CASE("pin manager forwards pin errors and recovers only hidden windows") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    PinManager manager;
    const auto hidden = manager.create(makeDocument({40, 20}), {-900, 20});
    const auto visible = manager.create(makeDocument({40, 20}), {10, 20});
    REQUIRE(hidden.id.has_value());
    REQUIRE(visible.id.has_value());
    auto* hiddenWindow = manager.window(*hidden.id);
    auto* visibleWindow = manager.window(*visible.id);
    REQUIRE(hiddenWindow != nullptr);
    REQUIRE(visibleWindow != nullptr);
    const auto visibleBefore = visibleWindow->geometry();
    manager.recoverVisibility({{0, 0, 1920, 1040}});
    CHECK(QRect{0, 0, 1920, 1040}.intersects(hiddenWindow->geometry()));
    CHECK(visibleWindow->geometry() == visibleBefore);

    QString error;
    QObject::connect(&manager, &PinManager::errorOccurred,
                     [&error](const QString& message) { error = message; });
    emit hiddenWindow->errorOccurred(*hidden.id, QStringLiteral("save failed"));
    CHECK(error == QStringLiteral("save failed"));
}

TEST_CASE("pin manager makes a newly created offscreen pin immediately visible") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    PinManager manager({}, {}, [] { return QList<QRect>{{0, 0, 1600, 1000}}; });

    const auto result = manager.create(makeDocument({800, 500}), {2400, 1400});

    REQUIRE(result.id.has_value());
    auto* window = manager.window(*result.id);
    REQUIRE(window != nullptr);
    CHECK(window->isVisible());
    CHECK(QRect{0, 0, 1600, 1000}.intersects(window->geometry()));
}

TEST_CASE("pin manager destruction leaves no managed top level window") {
    auto& application = annotationTestApplication();
    Q_UNUSED(application);
    QPointer<lc::pin::PinWindow> guard;
    {
        auto manager = std::make_unique<PinManager>();
        const auto result = manager->create(makeDocument(), {});
        REQUIRE(result.id.has_value());
        guard = manager->window(*result.id);
        REQUIRE(guard != nullptr);
    }
    CHECK(guard == nullptr);
}

TEST_CASE("tray contract tracks manager count and closes every pin") {
    auto& application = annotationTestApplication();
    lc::app::AppController controller(application);
    PinManager manager;
    QObject::connect(&manager, &PinManager::countChanged, &controller,
                     &lc::app::AppController::setPinCount);
    QObject::connect(&controller, &lc::app::AppController::closeAllPinsRequested, &manager,
                     &PinManager::closeAll);
    auto* closeAllPins = controller.findChild<QAction*>("closeAllPinsAction");
    REQUIRE(closeAllPins != nullptr);
    CHECK_FALSE(closeAllPins->isEnabled());

    REQUIRE(manager.create(makeDocument(), {}).id.has_value());
    REQUIRE(manager.create(makeDocument(), {}).id.has_value());
    CHECK(closeAllPins->isEnabled());

    closeAllPins->trigger();

    CHECK(manager.count() == 0);
    CHECK_FALSE(closeAllPins->isEnabled());
}
} // namespace
