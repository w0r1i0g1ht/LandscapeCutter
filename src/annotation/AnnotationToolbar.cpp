#include "annotation/AnnotationToolbar.hpp"

#include "annotation/AnnotationDocument.hpp"
#include "annotation/AnnotationInteraction.hpp"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>

#include <algorithm>
#include <type_traits>

namespace lc::annotation {
namespace {
const AnnotationObject* selectedAnnotation(const AnnotationDocument* document) {
    if (!document || !document->selectedId().has_value())
        return nullptr;
    const auto id = *document->selectedId();
    const auto found = std::find_if(document->objects().begin(), document->objects().end(),
                                    [id](const auto& object) { return object.id == id; });
    return found == document->objects().end() ? nullptr : &*found;
}

std::optional<AnnotationStyle> selectedAnnotationStyle(const AnnotationDocument* document) {
    const auto* object = selectedAnnotation(document);
    if (!object)
        return std::nullopt;
    return std::visit(
        [](const auto& value) -> std::optional<AnnotationStyle> {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, MosaicAnnotation>)
                return std::nullopt;
            else
                return value.style;
        },
        object->payload);
}

std::optional<int> selectedMosaicBlockSize(const AnnotationDocument* document) {
    const auto* object = selectedAnnotation(document);
    if (!object)
        return std::nullopt;
    if (const auto* mosaic = std::get_if<MosaicAnnotation>(&object->payload))
        return mosaic->blockSize;
    return std::nullopt;
}

QToolButton* addButton(QWidget* parent, QHBoxLayout& layout, const QString& name,
                       const QString& text) {
    auto* button = new QToolButton(parent);
    button->setObjectName(name);
    button->setText(text);
    button->setFocusPolicy(Qt::NoFocus);
    layout.addWidget(button);
    return button;
}
} // namespace

AnnotationToolbar::AnnotationToolbar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    const auto addTool = [this, layout](const QString& name, const QString& text,
                                        const QString& toolTip, AnnotationTool tool) {
        auto* button = addButton(this, *layout, name, text);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setToolTip(toolTip);
        connect(button, &QToolButton::clicked, this, [this, tool] {
            if (document_ && tool != AnnotationTool::Select)
                document_->clearSelection();
            emit toolRequested(tool);
        });
        return button;
    };

    selectToolButton_ = addTool(QStringLiteral("selectToolButton"), tr("编辑"),
                                tr("选择、移动或调整已有标注"), AnnotationTool::Select);
    rectangleToolButton_ = addTool(QStringLiteral("rectangleToolButton"), tr("矩形"),
                                   tr("绘制矩形标注"), AnnotationTool::Rectangle);
    ellipseToolButton_ = addTool(QStringLiteral("ellipseToolButton"), tr("椭圆"),
                                 tr("绘制椭圆标注"), AnnotationTool::Ellipse);
    arrowToolButton_ = addTool(QStringLiteral("arrowToolButton"), tr("箭头"),
                               tr("绘制箭头标注"), AnnotationTool::Arrow);
    brushToolButton_ = addTool(QStringLiteral("brushToolButton"), tr("画笔"),
                               tr("自由绘制标注"), AnnotationTool::Freehand);
    textToolButton_ = addTool(QStringLiteral("textToolButton"), tr("文字"),
                              tr("添加文字标注"), AnnotationTool::Text);
    mosaicToolButton_ = addTool(QStringLiteral("mosaicToolButton"), tr("马赛克"),
                                tr("对区域添加马赛克"), AnnotationTool::Mosaic);

    colorButton_ = addButton(this, *layout, QStringLiteral("colorButton"), tr("颜色"));
    colorButton_->setToolTip(tr("设置标注颜色"));
    connect(colorButton_, &QToolButton::clicked, this, [this] {
        if (!interaction_)
            return;
        const auto selectedStyle = interaction_->tool() == AnnotationTool::Select
                                       ? selectedAnnotationStyle(document_)
                                       : std::nullopt;
        const auto color = QColorDialog::getColor(
            selectedStyle.value_or(interaction_->style()).color, this, tr("选择颜色"));
        if (!color.isValid())
            return;
        auto style = selectedStyle.value_or(interaction_->style());
        style.color = color;
        interaction_->setStyle(style);
        emit annotationChanged();
    });

    lineWidth_ = new QDoubleSpinBox(this);
    lineWidth_->setObjectName(QStringLiteral("lineWidthSpinBox"));
    lineWidth_->setRange(1.0, 64.0);
    lineWidth_->setValue(3.0);
    lineWidth_->setDecimals(1);
    lineWidth_->setPrefix(tr("线宽 "));
    lineWidth_->setSuffix(tr(" px"));
    lineWidth_->setToolTip(tr("矩形、椭圆、箭头和画笔的线条宽度"));
    lineWidth_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(lineWidth_);
    connect(lineWidth_, &QDoubleSpinBox::valueChanged, this, [this](double width) {
        if (!interaction_)
            return;
        const auto selectedStyle = interaction_->tool() == AnnotationTool::Select
                                       ? selectedAnnotationStyle(document_)
                                       : std::nullopt;
        auto style = selectedStyle.value_or(interaction_->style());
        style.physicalSize = width;
        interaction_->setStyle(style);
        emit annotationChanged();
    });

    fontSize_ = new QSpinBox(this);
    fontSize_->setObjectName(QStringLiteral("fontSizeSpinBox"));
    fontSize_->setRange(1, 256);
    fontSize_->setValue(24);
    fontSize_->setPrefix(tr("字号 "));
    fontSize_->setSuffix(tr(" px"));
    fontSize_->setToolTip(tr("文字标注的字号"));
    fontSize_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(fontSize_);
    connect(fontSize_, &QSpinBox::valueChanged, this, [this](const int pixelSize) {
        if (document_ && interaction_ && interaction_->tool() == AnnotationTool::Select) {
            const auto* selected = selectedAnnotation(document_);
            if (selected && std::holds_alternative<TextAnnotation>(selected->payload)) {
                auto replacement = *selected;
                std::get<TextAnnotation>(replacement.payload).style.physicalSize = pixelSize;
                static_cast<void>(document_->replaceObject(std::move(replacement)));
            }
        }
        emit textSizeChanged(pixelSize);
        emit annotationChanged();
    });

    mosaicBlockSize_ = new QSpinBox(this);
    mosaicBlockSize_->setObjectName(QStringLiteral("mosaicBlockSizeSpinBox"));
    mosaicBlockSize_->setRange(1, 128);
    mosaicBlockSize_->setValue(12);
    mosaicBlockSize_->setPrefix(tr("块大小 "));
    mosaicBlockSize_->setSuffix(tr(" px"));
    mosaicBlockSize_->setToolTip(tr("马赛克像素块的大小"));
    mosaicBlockSize_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(mosaicBlockSize_);
    connect(mosaicBlockSize_, &QSpinBox::valueChanged, this, [this](int blockSize) {
        if (interaction_)
            interaction_->setMosaicBlockSize(blockSize);
    });

    undoButton_ = addButton(this, *layout, QStringLiteral("undoButton"), tr("撤销"));
    redoButton_ = addButton(this, *layout, QStringLiteral("redoButton"), tr("重做"));
    deleteButton_ = addButton(this, *layout, QStringLiteral("deleteButton"), tr("删除"));
    copyButton_ = addButton(this, *layout, QStringLiteral("copyButton"), tr("复制"));
    saveButton_ = addButton(this, *layout, QStringLiteral("saveButton"), tr("保存"));
    pinButton_ = addButton(this, *layout, QStringLiteral("pinButton"), tr("贴图"));
    cancelButton_ = addButton(this, *layout, QStringLiteral("cancelButton"), tr("取消"));
    doneButton_ = addButton(this, *layout, QStringLiteral("doneButton"), tr("完成"));

    connect(undoButton_, &QToolButton::clicked, this, &AnnotationToolbar::undoRequested);
    connect(redoButton_, &QToolButton::clicked, this, &AnnotationToolbar::redoRequested);
    connect(deleteButton_, &QToolButton::clicked, this, &AnnotationToolbar::deleteRequested);
    connect(copyButton_, &QToolButton::clicked, this, &AnnotationToolbar::copyRequested);
    connect(saveButton_, &QToolButton::clicked, this, &AnnotationToolbar::saveRequested);
    connect(pinButton_, &QToolButton::clicked, this, &AnnotationToolbar::pinRequested);
    connect(cancelButton_, &QToolButton::clicked, this, &AnnotationToolbar::cancelRequested);
    connect(doneButton_, &QToolButton::clicked, this, &AnnotationToolbar::doneRequested);

    setMode(AnnotationToolbarMode::Snip);
    refresh();
}

void AnnotationToolbar::setMode(const AnnotationToolbarMode mode) {
    mode_ = mode;
    const bool snip = mode_ == AnnotationToolbarMode::Snip;
    pinButton_->setVisible(snip);
    cancelButton_->setVisible(snip);
    doneButton_->setVisible(!snip);
    refresh();
}

void AnnotationToolbar::setContentAvailable(const bool available) {
    contentAvailable_ = available;
    refresh();
}

void AnnotationToolbar::setContext(AnnotationDocument* document,
                                   AnnotationInteraction* interaction) {
    document_ = document;
    interaction_ = interaction;
    refresh();
}

void AnnotationToolbar::clearContext() {
    document_ = nullptr;
    interaction_ = nullptr;
    refresh();
}

void AnnotationToolbar::setBusy(const bool busy) {
    busy_ = busy;
    refresh();
}

int AnnotationToolbar::textPixelSize() const noexcept {
    return fontSize_->value();
}

void AnnotationToolbar::refresh() {
    const bool annotationActive = document_ && interaction_;
    const bool contentAvailable = contentAvailable_ || annotationActive;
    copyButton_->setEnabled(contentAvailable && !busy_);
    saveButton_->setEnabled(contentAvailable && !busy_);
    pinButton_->setEnabled(contentAvailable && !busy_);
    cancelButton_->setEnabled(true);
    doneButton_->setEnabled(!busy_);

    const auto tool = annotationActive ? interaction_->tool() : AnnotationTool::Select;
    const auto selectedStyle = annotationActive && tool == AnnotationTool::Select
                                   ? selectedAnnotationStyle(document_)
                                   : std::nullopt;
    const auto selectedBlockSize = annotationActive && tool == AnnotationTool::Select
                                       ? selectedMosaicBlockSize(document_)
                                       : std::nullopt;
    const auto* selectedObject = selectedAnnotation(document_);
    const bool selectedText =
        selectedObject && std::holds_alternative<TextAnnotation>(selectedObject->payload);
    const auto selectedTextStyle =
        selectedText ? selectedAnnotationStyle(document_) : std::nullopt;
    const bool usesColor = annotationActive &&
                           ((tool == AnnotationTool::Select && selectedStyle.has_value()) ||
                            (tool != AnnotationTool::Select && tool != AnnotationTool::Mosaic));
    const bool usesLineWidth = annotationActive &&
                               ((tool == AnnotationTool::Select && selectedStyle.has_value() &&
                                 !selectedText) ||
                                tool == AnnotationTool::Rectangle ||
                                tool == AnnotationTool::Ellipse || tool == AnnotationTool::Arrow ||
                                tool == AnnotationTool::Freehand);
    const bool usesFontSize =
        annotationActive && (tool == AnnotationTool::Text ||
                             (tool == AnnotationTool::Select && selectedText));

    for (auto* button : {selectToolButton_, rectangleToolButton_, ellipseToolButton_,
                         arrowToolButton_, brushToolButton_, textToolButton_, mosaicToolButton_}) {
        button->setVisible(contentAvailable);
        button->setEnabled(!busy_);
    }
    colorButton_->setVisible(usesColor);
    lineWidth_->setVisible(usesLineWidth);
    fontSize_->setVisible(usesFontSize);
    mosaicBlockSize_->setVisible(annotationActive &&
                                 (tool == AnnotationTool::Mosaic ||
                                  (tool == AnnotationTool::Select &&
                                   selectedBlockSize.has_value())));
    undoButton_->setVisible(annotationActive);
    redoButton_->setVisible(annotationActive);
    deleteButton_->setVisible(annotationActive);

    selectToolButton_->setChecked(tool == AnnotationTool::Select);
    rectangleToolButton_->setChecked(tool == AnnotationTool::Rectangle);
    ellipseToolButton_->setChecked(tool == AnnotationTool::Ellipse);
    arrowToolButton_->setChecked(tool == AnnotationTool::Arrow);
    brushToolButton_->setChecked(tool == AnnotationTool::Freehand);
    textToolButton_->setChecked(tool == AnnotationTool::Text);
    mosaicToolButton_->setChecked(tool == AnnotationTool::Mosaic);

    if (annotationActive) {
        const QSignalBlocker lineWidthBlocker(lineWidth_);
        const QSignalBlocker fontSizeBlocker(fontSize_);
        const QSignalBlocker mosaicBlocker(mosaicBlockSize_);
        lineWidth_->setValue(selectedStyle.value_or(interaction_->style()).physicalSize);
        if (selectedTextStyle.has_value())
            fontSize_->setValue(qMax(1, qRound(selectedTextStyle->physicalSize)));
        mosaicBlockSize_->setValue(selectedBlockSize.value_or(interaction_->mosaicBlockSize()));
        undoButton_->setEnabled(document_->canUndo() && !busy_);
        redoButton_->setEnabled(document_->canRedo() && !busy_);
        deleteButton_->setEnabled(document_->selectedId().has_value() && !busy_);
    }
}
} // namespace lc::annotation
