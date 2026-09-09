#include "annotation/AnnotationHistory.hpp"

#include <algorithm>

namespace lc::annotation {
void AnnotationHistory::push(ObjectCommand command) {
    commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(nextCommand_), commands_.end());
    commands_.push_back(std::move(command));
    if (commands_.size() > maximumCommands) {
        commands_.erase(commands_.begin());
    }
    nextCommand_ = commands_.size();
}

std::optional<ObjectCommand> AnnotationHistory::undo() {
    if (!canUndo()) {
        return std::nullopt;
    }
    --nextCommand_;
    return commands_[nextCommand_];
}

std::optional<ObjectCommand> AnnotationHistory::redo() {
    if (!canRedo()) {
        return std::nullopt;
    }
    return commands_[nextCommand_++];
}

bool AnnotationHistory::canUndo() const noexcept {
    return nextCommand_ != 0;
}

bool AnnotationHistory::canRedo() const noexcept {
    return nextCommand_ != commands_.size();
}
} // namespace lc::annotation
