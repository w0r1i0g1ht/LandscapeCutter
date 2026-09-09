#pragma once
#include "snip/SnapshotBatch.hpp"
#include <QThreadPool>

namespace lc::snip {
// Declared after the device manager; its destructor joins workers before device destruction.
class SnapshotReadback final : public QObject {
  public:
    explicit SnapshotReadback(graphics::d3d11::D3d11DeviceManager& manager) : manager_(manager) {
        pool_.setMaxThreadCount(1);
    }
    ~SnapshotReadback() override {
        pool_.waitForDone();
    }
    ReadbackFunction function();

  private:
    graphics::d3d11::D3d11DeviceManager& manager_;
    QThreadPool pool_;
};
} // namespace lc::snip
