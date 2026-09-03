# 里程碑 1：Windows 图形基础详细设计

## 1. 文档状态

- 状态：已批准，实施计划已制定
- 日期：2026-09-03
- 上位设计：[LandscapeCutter C++ 重构设计](2026-09-02-cpp-rewrite-design.md)
- 前置里程碑：[里程碑 0：C++ 工程基础](../plans/2026-09-02-milestone-0-cpp-foundation.md)

本文细化里程碑 1，不改变上位设计已经确定的产品定位、工程基线和后续里程碑边界。

## 2. 目标与退出条件

里程碑 1 建立可供静态截图和实时贴图共同复用的 Windows 图形基础：

- 建立全进程共享的 D3D11 设备；
- 使用 Windows Graphics Capture（WGC）捕获指定显示器的单帧；
- 建立支持多显示器、负坐标和混合 DPI 的显示器模型；
- 在进程创建 Qt 窗口前启用 Per-Monitor V2 DPI；
- 注册全局 `F1`，并提供同语义的托盘菜单入口；
- 保证同一 Windows 会话中只运行一个 LandscapeCutter 实例；
- 将第一张有效捕获帧标准化为项目自有 GPU 纹理，并只保存在内存中。

退出条件：用户把鼠标放到任意已连接显示器并按 `F1` 后，程序能捕获该显示器的一帧，托盘
通知显示正确的显示器与物理像素尺寸；资源测试、自动测试和真实桌面验收全部通过。

## 3. 本阶段用户行为

### 3.1 单帧捕获

`F1` 的行为固定为：

1. 在处理触发的时刻读取鼠标位置；
2. 选择鼠标所在显示器；
3. 临时建立该显示器的 WGC 捕获会话；
4. 接收并复制第一张有效 GPU 帧；
5. 立即关闭捕获会话和帧池；
6. 在内存中替换上一张成功帧；
7. 显示包含显示器名称、物理尺寸和像素格式的托盘通知。

本阶段不显示截图预览，不创建选区，不写剪贴板，也不保存图片。这里的“捕获”是图形链路
验收，不是面向用户的完整截图工作流。

### 3.2 防重入

捕获状态只有 `Idle` 和 `Capturing`。处于 `Capturing` 时再次按 `F1` 或点击菜单，不创建新
会话、不排队，显示一次“正在捕获”通知。每次请求最多等待第一帧 2 秒。

### 3.3 托盘菜单

托盘菜单在现有“退出”之前增加“捕获当前显示器单帧”。菜单入口与 `F1` 进入同一个
`CaptureCoordinator::requestCapture()`，不得维护第二套捕获逻辑。

### 3.4 快捷键冲突

默认注册无修饰键的 `F1`，并使用 `MOD_NOREPEAT` 避免按住按键产生系统重复消息。注册失败
时程序继续驻留托盘，明确提示快捷键冲突，菜单捕获仍可用。快捷键自定义留到里程碑 6。
若 WGC、显示器目录或 D3D11 完全不可用，则不注册 `F1` 并禁用菜单捕获项；这与“只有热键
冲突、捕获能力仍可用”的降级状态严格区分。

### 3.5 再次启动

第二次启动不创建 Qt 应用、托盘、D3D11 设备或捕获资源。它只向第一个实例发送“再次启动”
信号，然后退出；第一个实例显示“LandscapeCutter 已在运行”通知。

## 4. 总体架构

```text
F1 / 托盘菜单
      │
      ▼
CaptureCoordinator              app：业务状态与用户反馈
      │
      ├──────────────┐
      ▼              ▼
DisplayCatalog   MonitorCaptureService
platform/windows     capture/windows
                         │
                         ▼
                 D3d11DeviceManager
                    graphics/d3d11
                         │
                         ▼
                   WGC + D3D11
```

边界规则：

- `app` 只编排流程和产生用户通知，不实现 Win32、WinRT 或 D3D11 细节；
- `platform/windows` 封装 DPI、显示器、热键、原生消息和单实例；
- `capture/windows` 是唯一允许持有 WGC 对象的模块；
- `graphics/d3d11` 管理设备、立即上下文、自有纹理和设备代际；
- WGC 帧对象不得离开 `capture/windows`；业务层只接收标准 `CaptureFrame`；
- 捕获服务不得直接显示 UI，所有成功和错误都作为结果返回 `app`。

## 5. 核心类型与接口契约

命名空间以 `lc` 为根。以下签名表达必须保持的模块契约；实现计划可以补充私有成员，但不得
改变含义。

### 5.1 显示器类型

```cpp
namespace lc::platform {

struct MonitorId final {
    std::string value;
    auto operator<=>(const MonitorId&) const = default;
};

struct PhysicalRect final {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct PixelSize final {
    std::uint32_t width;
    std::uint32_t height;
};

}  // namespace lc::platform
```

`PhysicalRect` 使用左闭右开、上闭下开的物理像素区间，允许 `left` 和 `top` 为负数。
`MonitorId::value` 使用确定性的 UTF-8 文本，由适配器 LUID、target ID 和 monitor device path
组合生成，在一次进程运行期间稳定；`HMONITOR` 只作为 `MonitorDescriptor` 的瞬时原生句柄，
不能持久化为业务标识。

```cpp
namespace lc::platform::windows {

struct MonitorDescriptor final {
    MonitorId id;
    std::wstring displayName;
    std::wstring gdiDeviceName;
    HMONITOR nativeHandle;
    PhysicalRect desktopRect;
    PhysicalRect workRect;
    std::uint32_t dpiX;
    std::uint32_t dpiY;
    double scaleFactor;
    bool primary;
    bool hdrEnabled;
    LUID adapterId;
    std::uint32_t targetId;
    std::uint64_t catalogGeneration;
};

class DisplayCatalog final {
public:
    using RefreshResult = std::variant<std::vector<MonitorDescriptor>, DisplayError>;

    RefreshResult refresh();
    std::optional<MonitorDescriptor> monitorFromPoint(POINT point) const;
    std::uint64_t generation() const noexcept;
};

}  // namespace lc::platform::windows
```

`RefreshResult` 在成功时携带完整的新目录，失败时携带 `DisplayError`。不得发布部分刷新结果。

### 5.2 捕获帧

```cpp
namespace lc::capture {

enum class CapturePixelFormat {
    Bgra8Unorm,
    Rgba16Float,
};

struct CaptureFrame final {
    winrt::com_ptr<ID3D11Texture2D> texture;
    platform::PixelSize size;
    CapturePixelFormat pixelFormat;
    std::optional<std::chrono::nanoseconds> systemRelativeTime;
    platform::MonitorId sourceMonitor;
    std::uint64_t displayGeneration;
    std::uint64_t deviceGeneration;
};

}  // namespace lc::capture
```

`CaptureFrame::texture` 必须是项目新建并复制的纹理，不得直接引用已关闭帧池拥有的表面。
帧对象可移动、不可复制。尺寸来自收到的 WGC `ContentSize`，并与纹理描述核对。

### 5.3 捕获结果

```cpp
namespace lc::capture {

enum class CaptureErrorCode {
    Unsupported,
    MonitorUnavailable,
    AccessDenied,
    Timeout,
    EmptyFrame,
    DisplayChanged,
    DeviceLost,
    DeviceCreationFailed,
    Cancelled,
    Internal,
};

struct CaptureError final {
    CaptureErrorCode code;
    std::error_code nativeCode;
};

using CaptureResult = std::variant<CaptureFrame, CaptureError>;

}  // namespace lc::capture
```

错误对象不携带截图数据、窗口标题或用户内容。可展示的中文文案在 `app` 层映射，不把界面
文案写入底层错误类型。

### 5.4 捕获服务

```cpp
namespace lc::capture {

struct MonitorCaptureRequest final {
    platform::windows::MonitorDescriptor monitor;
    std::chrono::milliseconds timeout{2000};
};

using CaptureCompletion = std::function<void(CaptureResult)>;

class IMonitorCaptureService {
public:
    virtual ~IMonitorCaptureService() = default;
    virtual void captureOnce(MonitorCaptureRequest request,
                             CaptureCompletion completion) = 0;
    virtual void cancel() noexcept = 0;
};

}  // namespace lc::capture
```

completion 自身必须可复制，但 `CaptureResult` 按值移动进入回调；任何路径都只能调用一次。

## 6. 显示器模型与 DPI

### 6.1 进程 DPI 模式

可执行文件 manifest 声明 `PerMonitorV2`。`main()` 在构造 `QApplication` 前调用并验证
`SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`；如果 DPI 模式
已经由 manifest 设置，则验证当前上下文确实等价于 Per-Monitor V2。无法建立正确模式时记录
错误并停止启动，避免在错误坐标体系中静默捕获。

### 6.2 枚举与映射

`DisplayCatalog::refresh()` 组合以下来源：

- `EnumDisplayMonitors` 与 `GetMonitorInfoW`：`HMONITOR`、桌面矩形、工作区和主屏状态；
- `QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS)`：活动 source/target 路径；
- `DisplayConfigGetDeviceInfo`：GDI 设备名、友好名、monitor device path 和 HDR 状态；
- `GetDpiForMonitor(MDT_EFFECTIVE_DPI)`：每个显示器的有效 DPI。

通过 `MONITORINFOEXW::szDevice` 与 display-config source GDI 名称连接两套结果。任何无法唯一
映射的活动显示器使本次刷新失败，不发布半完整目录。

`WM_DISPLAYCHANGE`、相关设备变化或恢复事件触发防抖刷新。刷新成功后 generation 加一；
刷新失败时保留上一个只读目录，但拒绝发起新捕获并通知用户。

### 6.3 鼠标所在显示器

触发时使用 `GetCursorPos` 和 `MonitorFromPoint(..., MONITOR_DEFAULTTONEAREST)` 获取
`HMONITOR`，再映射到当前目录。显示器边界、负坐标和 DPI 不通过 Qt 逻辑坐标换算。

## 7. D3D11 设备模型

`D3d11DeviceManager` 在进程内只发布一个当前设备代际：

1. 使用 `D3D_DRIVER_TYPE_HARDWARE` 创建 D3D11 设备；
2. 始终启用 `D3D11_CREATE_DEVICE_BGRA_SUPPORT`；
3. 接受 feature level 11.1 或 11.0；
4. 调试构建可以尝试 debug layer，但缺少 Graphics Tools 时必须自动重试无 debug-layer 配置；
5. 硬件设备失败后使用 `D3D_DRIVER_TYPE_WARP`；
6. 两者均失败则返回 `DeviceCreationFailed`，捕获功能禁用而托盘继续运行。

设备管理器同时创建供 WGC 使用的 WinRT `IDirect3DDevice`。立即上下文的复制和状态查询通过
同一个互斥边界串行化；不得让 Qt 主线程与帧回调并发调用同一立即上下文。

每次成功建设或重建设备，`deviceGeneration` 加一。旧代际捕获结果即使随后到达也必须丢弃。

设备丢失时：取消活动捕获、释放与旧设备有关的资源、重建设备并只重试当前请求一次。重试
失败后禁用捕获，等待未来显式触发恢复，而不是无限循环。

## 8. Windows Graphics Capture 单帧流程

### 8.1 捕获项创建

使用 `IGraphicsCaptureItemInterop::CreateForMonitor` 从目标 `HMONITOR` 创建
`GraphicsCaptureItem`，不显示系统 picker。先验证 `GraphicsCaptureSession::IsSupported()`。

### 8.2 像素格式

- 普通 SDR 显示器：`DirectXPixelFormat::B8G8R8A8UIntNormalized` /
  `DXGI_FORMAT_B8G8R8A8_UNORM`；
- HDR 已启用的显示器：`DirectXPixelFormat::R16G16B16A16Float` /
  `DXGI_FORMAT_R16G16B16A16_FLOAT`。

里程碑 1 不执行 HDR 到 SDR 的色调映射。该转换属于里程碑 2 的导出设计。

### 8.3 帧池与所有权

使用 `Direct3D11CaptureFramePool::CreateFreeThreaded`，缓冲数固定为 2。
`FrameArrived` 在非 Qt 主线程触发：

1. 原子地取得一次处理权，后续帧直接忽略；
2. 调用 `TryGetNextFrame()`；
3. 验证 `ContentSize` 和表面纹理描述；
4. 创建同尺寸、同格式的项目自有 D3D11 纹理；
5. 在设备管理器的串行边界内 `CopyResource`；
6. 撤销事件 token，关闭 session、frame 和 frame pool；
7. 将 `CaptureResult` 通过 Qt queued connection 交回主线程。

捕获完成、超时、取消和析构只能有一个路径赢得 completion；任何晚到回调都不得访问已释放
对象或再次调用 completion。

## 9. 应用协调与通知

`CaptureCoordinator` 位于 Qt 主线程，依赖 `DisplayCatalog`、`D3d11DeviceManager` 和
`IMonitorCaptureService`。它负责：

- 将 `F1` 与托盘菜单统一为 `requestCapture()`；
- 检查 `Idle/Capturing`；
- 读取鼠标并生成 `MonitorCaptureRequest`；
- 捕获开始时记录 display/device generation；
- 丢弃代际不匹配的结果；
- 成功时移动保存 `latestFrame`；
- 失败时保留上一张成功帧；
- 把错误码映射为用户通知。

成功通知格式：

```text
已捕获 <显示器友好名>：<宽> × <高>（SDR BGRA8）
已捕获 <显示器友好名>：<宽> × <高>（HDR RGBA16F）
```

通知和日志可以包含显示器友好名、物理尺寸、像素格式、耗时和错误码，不得包含 GPU 像素、
截图、窗口画面、剪贴板内容或未来标注文本。

## 10. 原生消息与全局快捷键

`NativeMessageWindow` 在 Qt 主线程创建不可见的顶层原生窗口，封装窗口类注册、`HWND`
生命周期和静态 WndProc 转发。`GlobalHotkeyService` 使用固定 hotkey ID 注册 `F1`，并在收到
`WM_HOTKEY` 后通过 Qt signal 调用协调器。

该窗口必须是不可见的顶层 `WS_POPUP`/`WS_EX_TOOLWINDOW` 窗口，而不是 `HWND_MESSAGE`：
message-only window 不接收系统广播的显示器变化消息。显示器变化消息由此窗口转发给
`DisplayCatalog`，但原生窗口本身不解释业务状态。关闭时先 `UnregisterHotKey`，再销毁
消息窗口。

## 11. 单实例协议

同一用户会话内使用带固定 GUID 后缀的两个 `Local\` 内核对象：

- 自动复位命名 event：再次启动信号；
- 命名 mutex：主实例所有权。

event 在 mutex 之前创建，消除第二实例发现 mutex 后却找不到 event 的竞态。主实例持有 mutex，
并在 `QApplication` 建立后使用 `QWinEventNotifier` 监听 event。第二实例发现 mutex 已存在时
调用 `SetEvent` 后立即退出；即使主实例的 Qt 事件循环尚未建立，自动复位 event 也会保留
signaled 状态直到首次等待。

测试可以向构造函数传入唯一命名空间后缀，正式程序只能使用固定产品 GUID，避免并行测试
互相干扰或测试连接到真实运行实例。

## 12. 启动与关闭顺序

### 12.1 启动

```text
初始化 C++/WinRT apartment
→ 启用并验证 Per-Monitor V2 DPI
→ 建立单实例 event 与 mutex
→ 若为第二实例：SetEvent 并退出
→ 创建 QApplication
→ 创建托盘与原生消息窗口
→ 枚举显示器
→ 建立 D3D11 设备（硬件或 WARP）
→ 注册 F1
→ 进入 Qt 事件循环
```

显示器、D3D11 或快捷键初始化失败不影响托盘“退出”。只有无法保证 DPI 坐标正确、无法建立
单实例协议或现有必需图标资源损坏时，程序才在事件循环前失败退出。

### 12.2 关闭

```text
禁用捕获入口
→ 注销 F1
→ 取消活动捕获并阻止新 completion
→ 撤销 WGC 回调并关闭 session/frame pool
→ 释放 latestFrame
→ 释放 WinRT D3D device、D3D11 context/device
→ 停止单实例 event 监听并释放内核句柄
→ 移除托盘图标
→ 退出 Qt 事件循环
```

所有 COM 接口、WinRT 对象、Win32 句柄、Qt 连接和事件 token 使用 RAII 管理。

## 13. 错误与恢复矩阵

| 情况 | 当前请求 | 上一成功帧 | 后续捕获 | 用户反馈 |
|---|---|---|---|---|
| `F1` 注册冲突 | 不适用 | 保留 | 菜单可用 | 启动时提示冲突 |
| WGC 不支持 | 拒绝 | 保留 | 禁用 | 系统不支持捕获 |
| 显示器目录刷新失败 | 拒绝 | 保留 | 刷新成功前禁用 | 显示器信息不可用 |
| 目标显示器消失 | 失败 | 保留 | 可重新选择 | 显示器已变化 |
| 2 秒无有效帧 | 超时 | 保留 | 可重试 | 捕获超时 |
| 空帧或尺寸非法 | 失败 | 保留 | 可重试 | 捕获结果无效 |
| 设备丢失 | 取消 | 释放旧设备纹理 | 重建设备并重试一次 | 恢复失败时提示 |
| 硬件设备创建失败 | 尚未开始 | 保留 | 自动用 WARP | 提示兼容模式 |
| 硬件与 WARP 均失败 | 拒绝 | 释放设备相关帧 | 禁用 | 图形设备不可用 |
| 正在捕获时重复触发 | 忽略 | 保留 | 当前请求继续 | 正在捕获 |
| 程序退出 | 取消且不回调 UI | 释放 | 禁止 | 无额外通知 |

## 14. 测试设计

### 14.1 纯逻辑单元测试

- 负坐标矩形和左闭右开边界；
- 鼠标点到显示器的选择；
- DPI 与 scale factor 数据；
- SDR/HDR 像素格式选择；
- `Idle/Capturing` 防重入；
- 成功帧替换和失败保留；
- display/device generation 过期结果丢弃；
- 设备丢失只重试一次；
- 每个错误码到通知类别的映射。

这些测试使用假的显示器目录、设备提供者和捕获服务，但断言协调器的真实状态与输出，不断言
mock 是否存在。

### 14.2 Windows 资源测试

- 在当前机器创建硬件 D3D11 设备；
- 强制创建 WARP 设备；
- 创建测试纹理、执行 `CopyResource`，释放源纹理后验证目标描述和受控回读；
- 用唯一对象名建立主/次实例，验证 event 只唤醒主实例一次；
- 在测试原生窗口上注册不与产品冲突的测试热键，验证触发和注销；
- 多轮建立和销毁对象后，无活动回调和测试创建的句柄残留。

### 14.3 真实桌面捕获测试

独立的 desktop-integration 测试程序枚举当前所有活动显示器，逐个捕获一帧并验证：

- `CaptureFrame::texture` 非空；
- WGC `ContentSize`、纹理描述和 `CaptureFrame::size` 一致；
- pixel format 与显示器 SDR/HDR 状态一致；
- 捕获会话关闭后自有纹理仍可执行一次受控 GPU 到 CPU 回读；
- 测试不保存或输出任何像素内容。

真实桌面测试使用 `desktop-integration` CTest label。默认 `windows-msvc-debug` test preset
明确排除该 label；另设 `windows-msvc-debug-desktop` preset 只在真实交互桌面中运行它。
缺少 HDR 显示器时自动测试 HDR 决策逻辑，不把硬件缺失伪装成 HDR 集成通过。

### 14.4 进程测试

- 启动两个产品进程，第二个在限定时间内退出，第一个收到再次启动信号；
- 占用 `F1` 后启动产品，产品继续运行、报告冲突并保留菜单捕获；
- 退出后产品进程、消息窗口、全局热键和命名对象均消失。

## 15. 人工验收

1. 鼠标分别放到每个已连接显示器，按 `F1`，核对通知中的显示器和物理像素尺寸；
2. 用托盘“捕获当前显示器单帧”重复同一验证；
3. 快速重复按 `F1`，确认只有一个活动捕获且程序保持响应；
4. 人为占用 `F1`，确认冲突提示和菜单降级入口；
5. 第二次启动程序，确认第二进程退出、现有进程显示“已在运行”；
6. 正常退出，确认无托盘残影、捕获回调、线程和句柄残留；
7. 记录从触发到第一帧的耗时，常规桌面以不超过 150 毫秒为目标；异常硬件记录实测值，
   不把未达到目标伪装成通过。

若当前机器没有多显示器、混合 DPI 或 HDR 硬件，对应真实硬件项记录为“环境不具备”，不能
写成“已通过”；负坐标、DPI 和格式逻辑仍必须由自动测试覆盖。

## 16. 代码与构建布局

```text
src/
├─ app/
│  ├─ AppController.*                 修改：菜单、通知和对象装配
│  └─ CaptureCoordinator.*            新增：捕获业务状态
├─ capture/
│  ├─ CaptureFrame.hpp                新增：标准 GPU 帧
│  ├─ CaptureResult.hpp               新增：错误与结果
│  ├─ MonitorCaptureService.hpp       新增：可替换服务接口
│  └─ windows/
│     ├─ GraphicsCaptureInterop.*     新增：WinRT/DXGI 接口转换
│     └─ MonitorCaptureService.*      新增：WGC 单帧实现
├─ graphics/
│  └─ d3d11/
│     ├─ D3d11DeviceManager.*         新增：设备、代际与恢复
│     └─ TextureCopy.*                新增：自有纹理复制
└─ platform/
   ├─ MonitorTypes.hpp                新增：平台中立 ID、矩形和尺寸
   └─ windows/
      ├─ DpiAwareness.*               新增：Per-Monitor V2
      ├─ DisplayCatalog.*             新增：显示器目录
      ├─ MonitorDescriptor.hpp        新增：Windows 显示器数据
      ├─ NativeMessageWindow.*        新增：Win32 消息入口
      ├─ GlobalHotkeyService.*        新增：RegisterHotKey
      └─ SingleInstanceCoordinator.*  新增：mutex/event 协议

resources/windows/
├─ LandscapeCutter.manifest           新增：DPI 与系统兼容声明
└─ LandscapeCutter.rc                 新增：manifest 资源

tests/
├─ app/
├─ capture/
├─ graphics/
├─ platform/windows/
└─ integration/windows/
```

使用 Windows SDK 自带的 C++/WinRT、WGC、D3D11 和 DXGI，链接 `windowsapp`、`d3d11`、
`dxgi`、`user32`、`shcore` 等系统库。不新增第三方 vcpkg 依赖，不改变 C++20、Qt 6.8.2、
Visual Studio 2026、Windows SDK 10.0.19041 下限、x64 和动态 Qt 的既有约束。

## 17. 明确非目标

里程碑 1 不实现：

- 截图选区和冻结背景界面；
- 正式的 GPU 帧到 `QImage` 导出链路；
- 剪贴板、PNG 或 JPEG 保存；
- 标注、静态贴图或实时贴图；
- 持续后台屏幕捕获；
- 窗口目标捕获；
- 快捷键配置界面；
- HDR 到 SDR 色调映射；
- 安装包或发布包。

这些能力分别进入里程碑 2、3、4、5 和 6。里程碑 1 不为展示进度而加入临时截图文件或
测试专用产品命令行参数。
