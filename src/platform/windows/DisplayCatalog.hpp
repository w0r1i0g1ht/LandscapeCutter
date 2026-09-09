#pragma once

#include "platform/windows/DisplayTopologySource.hpp"
#include "platform/windows/MonitorDescriptor.hpp"

#include <optional>
#include <variant>
#include <vector>

namespace lc::platform::windows {

class DisplayCatalog final {
  public:
    using RefreshResult = std::variant<std::vector<MonitorDescriptor>, DisplayError>;

    explicit DisplayCatalog(IDisplayTopologySource& source);
    RefreshResult refresh();
    std::optional<MonitorDescriptor> findByNativeHandle(HMONITOR handle) const;
    std::optional<MonitorDescriptor> monitorContaining(platform::PhysicalPoint point) const;
    std::optional<MonitorDescriptor> monitorFromPoint(POINT point) const;
    std::uint64_t generation() const noexcept;
    bool healthy() const noexcept;
    const std::vector<MonitorDescriptor>& monitors() const noexcept {
        return monitors_;
    }

  private:
    IDisplayTopologySource& source_;
    std::vector<MonitorDescriptor> monitors_;
    std::uint64_t generation_{0};
    bool healthy_{false};
};

} // namespace lc::platform::windows
