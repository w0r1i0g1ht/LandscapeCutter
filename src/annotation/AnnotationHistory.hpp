#pragma once

#include "annotation/AnnotationTypes.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace lc::annotation {
struct ObjectCommand {
    std::optional<AnnotationObject> before;
    std::optional<AnnotationObject> after;
    std::optional<AnnotationId> selectionBefore;
    std::optional<AnnotationId> selectionAfter;
};

class AnnotationHistory final {
  public:
    void push(ObjectCommand command);
    [[nodiscard]] std::optional<ObjectCommand> undo();
    [[nodiscard]] std::optional<ObjectCommand> redo();
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

  private:
    static constexpr std::size_t maximumCommands = 100;
    std::vector<ObjectCommand> commands_;
    std::size_t nextCommand_{};
};
} // namespace lc::annotation
