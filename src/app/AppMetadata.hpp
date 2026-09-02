#pragma once

#include <string_view>

namespace lc::app {

struct AppMetadata final {
    static constexpr std::string_view name{"LandscapeCutter"};
    static constexpr std::string_view organization{"LandscapeCutter"};
    static constexpr std::string_view version{"0.1.0-dev"};
};

} // namespace lc::app
