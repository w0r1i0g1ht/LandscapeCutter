#include "platform/MonitorTypes.hpp"

namespace lc::platform {

std::int64_t width(const PhysicalRect rect) noexcept {
    return static_cast<std::int64_t>(rect.right) - static_cast<std::int64_t>(rect.left);
}

std::int64_t height(const PhysicalRect rect) noexcept {
    return static_cast<std::int64_t>(rect.bottom) - static_cast<std::int64_t>(rect.top);
}

bool isValid(const PhysicalRect rect) noexcept {
    return rect.left < rect.right && rect.top < rect.bottom;
}

bool contains(const PhysicalRect rect, const PhysicalPoint point) noexcept {
    return isValid(rect) && rect.left <= point.x && point.x < rect.right &&
        rect.top <= point.y && point.y < rect.bottom;
}

}  // namespace lc::platform
