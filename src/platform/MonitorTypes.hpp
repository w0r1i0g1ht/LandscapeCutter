#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace lc::platform {

struct MonitorId final {
    std::string value;
    auto operator<=>(const MonitorId&) const = default;
};

struct PhysicalPoint final {
    std::int32_t x;
    std::int32_t y;
};

struct PhysicalRect final {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct PixelSize final {
    std::uint32_t width;
    std::uint32_t height;
};

std::int64_t width(PhysicalRect rect) noexcept;
std::int64_t height(PhysicalRect rect) noexcept;
bool isValid(PhysicalRect rect) noexcept;
bool contains(PhysicalRect rect, PhysicalPoint point) noexcept;

}  // namespace lc::platform
