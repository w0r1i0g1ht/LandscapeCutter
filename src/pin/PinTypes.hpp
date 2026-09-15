#pragma once

#include "annotation/AnnotationDocument.hpp"

#include <QPoint>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace lc::pin {
using PinId = std::uint64_t;

enum class PinWindowMode { Viewing, Editing, ChoosingSavePath, Exporting };

class PinWindow;

struct PinCreateResult {
    std::optional<PinId> id;
    std::unique_ptr<annotation::AnnotationDocument> rejectedDocument;
    QString error;
};

using CreatePinWindow = std::function<PinWindow*(PinId)>;
using CreatePin =
    std::function<PinCreateResult(std::unique_ptr<annotation::AnnotationDocument>&, QPoint)>;
} // namespace lc::pin
