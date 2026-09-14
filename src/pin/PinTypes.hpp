#pragma once

#include <cstdint>

namespace lc::pin {
using PinId = std::uint64_t;

enum class PinWindowMode { Viewing, Editing, ChoosingSavePath, Exporting };
} // namespace lc::pin
