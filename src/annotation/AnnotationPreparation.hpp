#pragma once

#include "snip/SnapshotImage.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace lc::snip {
using PrepareAnnotation = std::function<void(std::vector<FrozenMonitor>, QRect,
                                             std::shared_ptr<std::atomic_bool>,
                                             std::function<void(QImage)>)>;

void prepareAnnotation(std::vector<FrozenMonitor> monitors, QRect selection,
                       std::shared_ptr<std::atomic_bool> cancelled,
                       std::function<void(QImage)> completion);
} // namespace lc::snip
