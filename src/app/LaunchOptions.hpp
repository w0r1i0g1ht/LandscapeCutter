#pragma once

#include <span>
#include <string_view>

namespace lc::app {

enum class LaunchMode {
    Normal,
    SmokeTest,
};

[[nodiscard]] LaunchMode parseLaunchMode(std::span<const std::string_view> arguments);

} // namespace lc::app
