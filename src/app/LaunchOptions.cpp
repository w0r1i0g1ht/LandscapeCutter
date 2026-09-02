#include "app/LaunchOptions.hpp"

#include <algorithm>

namespace lc::app {

LaunchMode parseLaunchMode(const std::span<const std::string_view> arguments) {
    const auto smokeTest = std::ranges::find(arguments, "--smoke-test");
    return smokeTest == arguments.end() ? LaunchMode::Normal : LaunchMode::SmokeTest;
}

} // namespace lc::app
