#include "pin/PinManager.hpp"

#include <vector>

namespace lc::pin {
PinManager::PinManager(CreatePinWindow factory, ChoosePinSavePath savePathChooser,
                       AvailablePinScreens availableScreens, QObject* parent)
    : QObject(parent), factory_(std::move(factory)),
      availableScreens_(std::move(availableScreens)) {
    if (!factory_) {
        factory_ = [savePathChooser = std::move(savePathChooser)](const PinId id) {
            return new PinWindow(id, savePathChooser);
        };
    }
}

PinManager::~PinManager() {
    std::vector<PinWindow*> windows;
    windows.reserve(windows_.size());
    for (const auto& [id, window] : windows_) {
        Q_UNUSED(id);
        if (window)
            windows.push_back(window);
    }
    windows_.clear();
    for (PinWindow* window : windows) {
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->close();
        delete window;
    }
}

PinCreateResult PinManager::create(std::unique_ptr<annotation::AnnotationDocument> document,
                                   const QRect physicalSelection) {
    if (!document || document->snapshot().base.isNull()) {
        return {.rejectedDocument = std::move(document), .error = tr("无法创建贴图：图片为空。")};
    }
    const PinId id = nextId_++;
    PinWindow* window = factory_(id);
    if (window == nullptr) {
        return {.rejectedDocument = std::move(document), .error = tr("无法创建贴图窗口。")};
    }
    QList<PinScreenGeometry> screens;
    if (availableScreens_) {
        try {
            screens = availableScreens_();
        } catch (...) {
            // Screen discovery is best effort; the unscaled fallback remains usable.
        }
    }
    const auto initialRect =
        initialPinWindowRect(physicalSelection, document->snapshot().base.size(), screens);
    const QString error = window->attachDocument(document, initialRect);
    if (!error.isEmpty()) {
        delete window;
        return {.rejectedDocument = std::move(document), .error = error};
    }
    QList<QRect> availableGeometries;
    availableGeometries.reserve(screens.size());
    for (const auto& screen : screens) {
        if (!screen.availableLogicalGeometry.isEmpty())
            availableGeometries.push_back(screen.availableLogicalGeometry);
    }
    window->recoverVisibility(availableGeometries);

    windows_.emplace(id, window);
    connect(window, &PinWindow::closed, this, [this](const PinId closedId) { remove(closedId); });
    connect(window, &QObject::destroyed, this, [this, id] { remove(id); });
    connect(window, &PinWindow::errorOccurred, this,
            [this](const PinId, const QString& message) { emit errorOccurred(message); });
    emitCountChanged();
    return {.id = id};
}

void PinManager::close(const PinId id) {
    const auto found = windows_.find(id);
    if (found == windows_.end())
        return;
    if (found->second) {
        found->second->close();
        return;
    }
    remove(id);
}

void PinManager::closeAll() {
    std::vector<PinId> ids;
    ids.reserve(windows_.size());
    for (const auto& [id, window] : windows_) {
        Q_UNUSED(window);
        ids.push_back(id);
    }
    for (const PinId id : ids)
        close(id);
}

void PinManager::recoverVisibility(const QList<QRect>& availableGeometries) {
    for (auto it = windows_.begin(); it != windows_.end();) {
        if (!it->second) {
            const auto id = it->first;
            ++it;
            remove(id);
            continue;
        }
        it->second->recoverVisibility(availableGeometries);
        ++it;
    }
}

std::size_t PinManager::count() const noexcept {
    return windows_.size();
}

PinWindow* PinManager::window(const PinId id) const noexcept {
    const auto found = windows_.find(id);
    return found == windows_.end() ? nullptr : found->second.data();
}

void PinManager::remove(const PinId id) {
    if (windows_.erase(id) != 0)
        emitCountChanged();
}

void PinManager::emitCountChanged() {
    emit countChanged(count());
}
} // namespace lc::pin
