#pragma once

#include "platform/windows/DisplayTopologySource.hpp"

namespace lc::platform::windows {

class WindowsDisplayTopologySource final : public IDisplayTopologySource {
public:
    TopologyReadResult read() override;
};

}  // namespace lc::platform::windows
