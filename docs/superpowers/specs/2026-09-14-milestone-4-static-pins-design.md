# 里程碑 4：可编辑静态贴图详细设计

- 日期：2026-09-14
- 状态：待用户审阅
- 前置里程碑：[里程碑 3：标注系统](2026-09-09-milestone-3-annotation-system-design.md)
- 上位设计：[LandscapeCutter C++ 重构设计](2026-09-02-cpp-rewrite-design.md)

## 1. 目标

里程碑 4 在现有截图与标注闭环上增加类似 Snipaste 的静态贴图：用户可把当前选区或带标注的
截图变成无边框置顶窗口，创建成功后结束截图会话。多个贴图可以同时存在并独立移动、缩放、调节
透明度、复制、保存和关闭。贴图保留原图、矢量标注对象及撤销历史，用户可以再次显示工具栏并继续
编辑，而不是只能查看一张已经压平的图片。

本里程碑完成静态、CPU 图像驱动的贴图窗口。目标窗口绑定、持续捕获、GPU 裁剪和 60 FPS 实时
显示属于里程碑 5。

## 2. 已确认的产品决策

1. 点击“贴图”后，只有在贴图窗口创建成功时才关闭本次截图或标注界面。
2. 贴图创建失败时保留完整截图会话、选区、标注对象和撤销历史，并显示非阻塞错误。
3. 贴图不是压平图片。每个贴图拥有独立、可继续编辑的 `AnnotationDocument`。
4. 贴图使用 Qt Widgets 与 `QImage` 静态渲染，不在本里程碑新增 D3D 交换链或 WGC 会话。
5. 从 `SnipOverlay` 提取共享 `AnnotationToolbar`；截图界面与贴图窗口不维护两套工具栏逻辑。
6. 普通贴图状态用于移动、缩放和透明度控制；编辑状态用于创建和修改标注。
7. 双击贴图进入编辑状态。点击工具栏“完成”、再次双击空白区域或在无草稿时按 `Esc` 退出编辑。
8. 鼠标滚轮以光标位置为锚点等比例缩放，`Ctrl + 滚轮` 调节透明度，范围 10%–100%。
9. 复制和保存读取当前文档快照，不关闭贴图，也不修改撤销历史。

## 3. 范围

### 3.1 包含

- 从未标注选区和正在标注的文档创建贴图；
- 多个独立无边框置顶贴图窗口；
- 普通状态下的拖动、滚轮等比例缩放和透明度调节；
- 可显示或隐藏的共享标注工具栏；
- 六类标注工具、编辑工具、删除、撤销和重做；
- 贴图右键菜单：编辑、复制、另存为、恢复原始大小、恢复不透明、关闭；
- 托盘菜单“关闭全部贴图”；
- 显示器拓扑变化后的窗口可见性恢复；
- 应用退出时的同步清理；
- 纯逻辑测试及 Qt offscreen 窗口测试。

### 3.2 不包含

- 贴图状态持久化或应用重启后恢复；
- 旋转、翻转、裁剪、取色、OCR、序号工具或图片管理器；
- 把鼠标或键盘事件转发到源窗口；
- 目标窗口绑定和窗口相对坐标；
- 实时捕获、GPU 纹理裁剪、交换链渲染和帧率控制；
- 静态贴图之间共享撤销历史；
- 贴图关闭后的恢复或回收站。

## 4. 用户交互

### 4.1 创建入口

截图工具栏在“复制”和“保存”旁增加“贴图”按钮。该按钮在存在有效选区或已进入标注状态时
可用，在准备、导出或重复创建期间禁用。

若文字编辑器正在编辑非空内容，点击“贴图”先按现有规则提交文字；空白文字关闭编辑器但不创建
对象。创建过程中工具栏进入忙碌状态，并忽略快捷键或重复按钮产生的第二次贴图请求。

从普通选区创建时，`SnipSession` 复用 `PrepareAnnotation` 的冻结选区合成边界，得到拥有、DPR 为
1 的 RGB32 `QImage`，再创建空的 `AnnotationDocument`。从标注状态创建时，直接转移现有文档及其
历史，不重新合成底图。

### 4.2 普通贴图状态

- 窗口使用 `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool`，不出现在任务栏；
- 左键拖动任意图片区域移动窗口；
- 滚轮以光标对应的桌面点为锚点缩放，图片宽高比保持不变；
- `Ctrl + 滚轮` 每个滚轮步进改变 5% 透明度，最终值限制在 10%–100%；
- 最小显示边长为 32 个设备独立像素，最大尺寸不设置人为倍数上限，但至少保留 32 × 32 的窗口
  区域位于某个可用屏幕内；
- 双击进入编辑状态；
- 右键弹出贴图菜单；
- 激活一个贴图不会改变其他贴图的层级属性、透明度或编辑状态。

滚轮缩放不修改原始图像或标注坐标。窗口只保存显示尺寸，绘制时使用文档到窗口的统一变换。
“恢复原始大小”把窗口客户区恢复为文档的物理像素尺寸；如果尺寸超过当前可用桌面，只移动窗口
保证可操作区域可见，不修改文档。

### 4.3 编辑状态

进入编辑状态后，贴图显示 `AnnotationToolbar`，并创建绑定当前文档的
`AnnotationInteraction`。图片区域的鼠标事件映射回文档物理像素坐标：

- 编辑工具选择、移动或调整已有标注；
- 矩形、椭圆、箭头、画笔、文字和马赛克沿用里程碑 3 的创建行为；
- 文字编辑器由当前 `PinWindow` 创建并跟随文档到窗口变换定位；
- `Ctrl+Z`、`Ctrl+Y` 和 `Delete` 作用于当前贴图；
- 编辑期间普通左键拖动交给标注交互，不移动贴图；
- 工具栏“完成”提交非空文字、取消未完成手势、清除对象选择并返回普通状态；
- 再次双击未命中标注的空白区域执行相同的完成行为；
- `Esc` 优先取消文字或标注草稿；没有草稿时退出编辑状态；
- 隐藏工具栏不合并对象，之后重新进入编辑仍可选中已有标注。

本里程碑不增加编辑状态下的窗口拖动手势。需要移动时先退出编辑，避免移动窗口与移动标注产生
歧义。

### 4.4 右键和托盘菜单

普通状态的右键菜单包含：

1. “编辑”——进入编辑状态；
2. “复制”——合成当前文档并写入剪贴板；
3. “另存为…”——使用现有 PNG/JPEG 选择与编码规则；
4. “恢复原始大小”——恢复文档物理像素大小；
5. “恢复不透明”——把透明度设为 100%；
6. “关闭”——关闭当前贴图。

编辑状态下右键仍由标注交互使用，不弹出窗口菜单；用户退出编辑后再使用菜单。托盘菜单新增
“关闭全部贴图”，仅在至少存在一个贴图时启用。

## 5. 架构与职责

### 5.1 `annotation/AnnotationToolbar`

新增可复用的 `QWidget`，只负责工具栏控件、可见状态和用户意图信号。它不持有截图会话、贴图
窗口或标注文档，也不直接执行文档命令。

公共接口采用以下形态：

```cpp
enum class AnnotationToolbarMode { Snip, Pin };

class AnnotationToolbar final : public QWidget {
    Q_OBJECT
  public:
    explicit AnnotationToolbar(QWidget* parent = nullptr);
    void setMode(AnnotationToolbarMode mode);
    void setContext(AnnotationDocument*, AnnotationInteraction*);
    void clearContext();
    void refresh();
    void setBusy(bool);
  signals:
    void toolRequested(AnnotationTool);
    void undoRequested();
    void redoRequested();
    void deleteRequested();
    void copyRequested();
    void saveRequested();
    void pinRequested();
    void doneRequested();
};
```

现有颜色、线宽和马赛克块大小控件迁入该组件。“取消”属于截图会话，“完成”属于贴图编辑状态，
因此宿主通过配置决定末尾动作的文本和信号含义。截图和贴图必须共用工具选择、属性读取、按钮状态
和待提交文字的外围协议。

### 5.2 `pin/PinGeometryModel`

新增不依赖窗口句柄的值类型，保存文档尺寸、当前窗口矩形和透明度，负责：

- 以桌面锚点为中心计算等比例缩放后的窗口矩形；
- 限制最小边长；
- 计算 5% 步进、10%–100% 范围的透明度；
- 把完全位于失效屏幕外的窗口移回可用区域；
- 恢复文档原始物理像素尺寸。

坐标计算使用 Qt 设备独立窗口坐标；标注文档仍使用 DPR 1 的物理像素。两者只通过
`PinWindow::documentToWindowTransform()` 转换，不把显示缩放写回文档。

### 5.3 `pin/PinWindow`

每个 `PinWindow` 是独立顶层 `QWidget`，独占：

- `std::unique_ptr<AnnotationDocument>`；
- 一个绑定该文档的 `AnnotationInteraction`；
- 一个 `AnnotationToolbar`；
- 一个 `PinGeometryModel`；
- 可选文字编辑器和编辑前文字对象；
- 当前普通/编辑/复制/保存状态。

`paintEvent` 先绘制文档底图，再通过现有 `drawAnnotations` 绘制对象和编辑草稿。普通状态不绘制
选择手柄；编辑状态沿用里程碑 3 的选中对象提示。复制和保存使用 `snapshot()` 与
`composeAnnotations()`，保持预览、剪贴板和文件输出一致。

窗口使用 `WA_DeleteOnClose`。关闭前取消未完成的保存或文字编辑，并保证后台导出任务不再回调已
销毁窗口。窗口发出 `closed(PinId)` 后由管理器移除登记项。

### 5.4 `pin/PinManager`

`PinManager` 由应用入口创建，生命周期覆盖整个 `QApplication::exec()`。它为贴图分配单调递增、
非零的 `PinId`，登记 `QPointer<PinWindow>`，并提供：

```cpp
struct PinCreateResult {
    std::optional<PinId> id;
    std::unique_ptr<AnnotationDocument> rejectedDocument;
    QString error;
};

class PinManager final : public QObject {
    Q_OBJECT
  public:
    PinCreateResult create(std::unique_ptr<AnnotationDocument> document,
                           QPoint preferredTopLeft);
    void closeAll();
    [[nodiscard]] std::size_t count() const noexcept;
  signals:
    void countChanged(std::size_t count);
    void errorOccurred(QString message);
};
```

成功结果设置 `id`，并保持 `rejectedDocument` 和 `error` 为空。失败结果不设置 `id`，把未消费的
文档放入 `rejectedDocument` 并填写 `error`；调用者据此恢复原会话。失败路径不得复制或丢弃文档。

### 5.5 `snip/SnipSession` 集成

`SnipOverlay` 增加 `pinRequested()`。`SnipSession` 增加 `pin()` 和可注入的 `CreatePin` 回调，
以便会话测试模拟成功与失败，不直接依赖真实顶层窗口。

会话状态增加 `PreparingPinFromSelection` 与 `CreatingPin`：

- `Selecting` 点击贴图后锁定选区，复用异步准备函数合成底图；
- `Annotating` 点击贴图后先由宿主提交文字，再进入 `CreatingPin`；
- 成功时转移文档、销毁 overlays、清空冻结图像并回到 `Idle`；
- 失败时取回文档并精确恢复 `Selecting` 或 `Annotating`；
- `cancel()`、显示失效或析构使准备请求过期，晚到回调不得创建贴图；
- 同一请求只能被提交一次。

创建贴图窗口发生在 GUI 线程。普通选区的图像合成继续在现有单线程工作池边界内执行。

### 5.6 应用集成

`main.cpp` 在 `SnipSession` 之前创建 `PinManager`，把创建回调注入会话，并把管理器错误连接到
`AppController::showErrorMessage`。`AppController` 托盘菜单增加“关闭全部贴图”动作，通过信号交给
管理器；`PinManager::countChanged` 控制动作启用状态。

显示配置变化时先取消截图会话，再通知 `PinManager` 使用更新后的可用屏幕矩形恢复贴图可见性。
应用退出顺序为：取消截图会话、关闭全部贴图、关闭捕获协调器、注销快捷键。

## 6. 所有权与一致性

`AnnotationDocument` 在任意时刻只有一个所有者：截图会话或一个贴图窗口。创建成功是所有权转移的
唯一提交点。失败、取消、显示失效和过期完成均不得复制或丢失文档。

从标注状态贴图时保留 `AnnotationHistory`，因此用户可在贴图中撤销贴图前创建的标注。转移前取消
未完成的手势并提交非空文字，保证文档中只有已提交对象。对象选择在进入普通贴图状态前清除，避免
隐藏工具栏时仍出现编辑手柄。

每次复制或保存都先在 GUI 线程取得不可变 `AnnotationSnapshot`，再把快照交给后台工作；后台任务
不访问 `PinWindow`、`AnnotationDocument` 或任何 QWidget。完成回调使用 `QPointer` 和请求 ID 拒绝
窗口关闭后的旧结果。

## 7. 多显示器与 DPI

- 文档尺寸和标注几何继续使用物理像素、DPR 1；
- 窗口位置和大小使用 Qt 顶层窗口的设备独立坐标；
- 初始位置优先放在选区中心附近，并限制至少 32 × 32 可操作区域处于目标屏幕可用几何内；
- 窗口拖到另一 DPI 显示器后，文档内容比例由 Qt 窗口 DPR 和统一变换重算，不修改标注数据；
- 显示器移除后，只移动完全不可见的贴图；仍有可操作区域可见的贴图保持用户位置；
- 本里程碑不对 HDR 图像增加新的色调映射，沿用截图阶段已经规范化的底图。

## 8. 错误和资源边界

- 空文档、空底图或超过 Qt 可分配范围的图像拒绝创建并返回中文错误；
- `PinWindow` 显示失败不关闭截图会话；
- 剪贴板或保存失败只提示错误，贴图和文档保持不变；
- 保存路径选择取消后返回原状态，不关闭工具栏或贴图；
- 关闭贴图取消其未提交导出，并等待最终提交同步边界后再释放相关状态；
- `PinManager::closeAll()` 可重复调用；窗口自身关闭和应用退出并发到达时只移除一次；
- 日志只记录贴图 ID、状态、尺寸和错误码，不记录图像内容、剪贴板内容、窗口画面或标注文字。

## 9. 测试策略与当前机器安全边界

### 9.1 允许的自动验证

里程碑 4 的开发验证只构建和运行不创建设备、不捕获桌面的目标：

- `PinGeometryModel` 纯逻辑测试；
- `AnnotationToolbar`、`PinWindow`、`PinManager` 的 `QT_QPA_PLATFORM=offscreen` 测试；
- `SnipSession` 使用注入回调和合成替身的状态机测试；
- 现有 annotation 与 app-controller 的离屏回归测试；
- 对指定非图形目标的编译，不运行 `landscapecutter_graphics_tests`、
  `landscapecutter_capture_tests` 或 `landscapecutter_desktop_capture_tests`。

关键行为包括：

- 缩放保持宽高比和桌面锚点，透明度正确限制；
- 多贴图创建、单独关闭、关闭全部和重复关闭；
- 创建成功转移文档及历史，失败恢复原会话；
- 选择合成取消或晚到时不创建贴图；
- 普通与编辑状态的事件路由互斥；
- 工具栏两种宿主的按钮、属性和文字提交一致；
- 编辑后预览、复制、PNG 和 JPEG 使用相同合成结果；
- 显示器几何变化只恢复完全不可见的贴图；
- 窗口关闭和应用退出没有顶层窗口残留。

### 9.2 暂缓的真实桌面验证

2026-09-14 当前开发机在合并后默认 CTest 运行期间发生 Windows
`VIDEO_MEMORY_MANAGEMENT_INTERNAL (0x0000010E)`。在独立转储分析明确风险前，不自动运行任何
真实桌面捕获、D3D、WGC 或包含这些目标的全量测试，也不尝试复现系统崩溃。

真实桌面人工验收在风险解除后单独执行，覆盖：多个贴图共存、置顶、跨屏拖动、100%/150%/200%
DPI、滚轮锚点缩放、透明度、编辑工具栏、复制保存和退出清理。每一组验收独立启动，不与捕获压力
测试混跑。

## 10. 退出条件

里程碑 4 需同时满足：

1. 普通选区和带标注截图都能创建贴图，成功后截图界面关闭；
2. 多个贴图稳定共存并可独立移动、缩放、调透明度和关闭；
3. 贴图可以重新进入编辑状态，六类工具、对象编辑、删除、撤销和重做可用；
4. 贴图前后的标注历史保持，隐藏工具栏后对象仍可再次编辑；
5. 预览、复制、PNG 和 JPEG 输出一致；
6. 创建失败、取消、显示失效和过期回调不丢失截图或创建幽灵贴图；
7. 关闭一个、关闭全部和应用退出均无贴图窗口或后台任务残留；
8. 本里程碑限定的纯逻辑与 Qt offscreen 自动测试全部通过；
9. 真实桌面验收只在 GPU 风险解除后补充，其暂缓状态必须在进度文档中明确记录。
