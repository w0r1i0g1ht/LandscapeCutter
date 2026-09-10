#include "annotation/AnnotationPreparation.hpp"

namespace lc::snip {
void prepareAnnotation(std::vector<FrozenMonitor> monitors, QRect selection,
                       std::shared_ptr<std::atomic_bool> cancelled,
                       std::function<void(QImage)> completion) {
    if (cancelled && cancelled->load(std::memory_order_acquire))
        return;
    auto image = composeSelection(monitors, selection);
    if (cancelled && cancelled->load(std::memory_order_acquire))
        return;
    completion(std::move(image));
}
} // namespace lc::snip
