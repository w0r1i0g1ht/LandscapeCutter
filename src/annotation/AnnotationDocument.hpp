#pragma once

#include "annotation/AnnotationHistory.hpp"

#include <optional>
#include <vector>

namespace lc::annotation {
class AnnotationDocument final {
  public:
    explicit AnnotationDocument(QImage base);

    [[nodiscard]] std::optional<AnnotationId> addObject(AnnotationPayload payload);
    [[nodiscard]] bool replaceObject(AnnotationObject object);
    [[nodiscard]] bool removeObject(AnnotationId id);
    [[nodiscard]] bool select(AnnotationId id) noexcept;
    void clearSelection() noexcept;
    [[nodiscard]] std::optional<AnnotationId> selectedId() const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] const std::vector<AnnotationObject>& objects() const noexcept;
    [[nodiscard]] AnnotationSnapshot snapshot() const;

  private:
    [[nodiscard]] std::optional<AnnotationPayload> normalize(AnnotationPayload payload) const;
    [[nodiscard]] std::vector<AnnotationObject>::iterator find(AnnotationId id) noexcept;
    [[nodiscard]] std::vector<AnnotationObject>::const_iterator find(AnnotationId id) const noexcept;
    void apply(const std::optional<AnnotationObject>& object,
               const std::optional<AnnotationObject>& displacedObject,
               std::optional<AnnotationId> selection);

    QImage base_;
    std::vector<AnnotationObject> objects_;
    std::optional<AnnotationId> selection_;
    AnnotationId nextId_{1};
    AnnotationHistory history_;
};
} // namespace lc::annotation
