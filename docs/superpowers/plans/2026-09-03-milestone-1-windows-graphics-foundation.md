# 里程碑 1：Windows 图形基础实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 建立可复用的 Windows 图形基础，使全局 `F2` 或托盘菜单能够捕获鼠标所在显示器
的一张 WGC/D3D11 GPU 帧，并报告正确的物理像素尺寸。

**架构：** `platform/windows` 管理 DPI、显示器、热键、原生消息与单实例，
`graphics/d3d11` 管理共享设备和纹理所有权，`capture/windows` 管理一次性 WGC 会话，
`app::CaptureCoordinator` 只编排状态和用户反馈。所有系统资源使用 RAII，捕获完成通过 Qt
队列返回主线程，业务层不持有 WinRT 捕获帧。

**技术栈：** C++20、Qt 6.8.2 Widgets、C++/WinRT、Windows Graphics Capture、D3D11、
DXGI、WIL、spdlog、Catch2、CMake 4.4+、Visual Studio 2026/MSVC 14.5x、x64。

**规格：** `docs/superpowers/specs/2026-09-03-milestone-1-windows-graphics-foundation-design.md`

## 全局约束

- 最低系统为 Windows 10 1903，SDK 下限为 Windows SDK 10.0.19041。
- 编译器为 Visual Studio 2026 MSVC 14.5x，语言标准保持 C++20，目标架构只支持 x64。
- Qt 固定为动态链接的 6.8.2；vcpkg 固定策略、tag object、peeled commit 和 bootstrap
  验证不得降低。
- 不增加第三方 vcpkg 依赖；只增加 Windows SDK 系统库。
- manifest 与运行时必须共同保证 Per-Monitor V2；不能依赖 Qt 逻辑坐标表示捕获区域。
- 普通帧使用 `DXGI_FORMAT_B8G8R8A8_UNORM`，HDR 帧使用
  `DXGI_FORMAT_R16G16B16A16_FLOAT`。
- WGC 会话按需建立，收到第一张有效帧后关闭；重复请求不并发、不排队。
- 捕获超时固定为 2000 ms；设备丢失最多重建设备并重试当前请求一次。
- WGC 对象只存在于 `capture/windows`；D3D11 立即上下文只能通过串行化图形边界使用。
- 本阶段不创建选区、不保存图片、不写剪贴板、不实现标注、贴图或持续后台捕获。
- 日志和测试输出不得包含截图像素、窗口画面、剪贴板内容或未来标注正文。
- 每个生产行为先写真实失败测试并观察 RED，再写最小实现得到 GREEN。
- 每个任务更新 `docs/superpowers/progress/2026-09-03-milestone-1-progress.md`，使用中文记录
  RED、GREEN、提交、审查和未闭合验收。
- 每个任务只提交本任务文件，不夹带本地 `.tools/`、`out/` 或无关修改。

---

### Task 1: 建立平台中立的显示器基础类型

**文件：**

- 新建：`src/platform/MonitorTypes.hpp`
- 新建：`src/platform/MonitorTypes.cpp`
- 新建：`tests/platform/MonitorTypesTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 新建：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：有符号物理点和左闭右开的物理矩形。
- 输出：`lc::platform::MonitorId`、`PhysicalPoint`、`PhysicalRect`、`PixelSize` 及纯函数
  `width()`、`height()`、`isValid()`、`contains()`。
- 后续任务只通过这些类型交换显示器 ID、桌面坐标与像素尺寸。

- [ ] **Step 1: 写物理坐标 RED 测试**

在 `MonitorTypesTests.cpp` 写出以下独立行为：

```cpp
TEST_CASE("physical rectangles support negative monitor coordinates") {
    const lc::platform::PhysicalRect rect{-1920, 0, 0, 1080};
    CHECK(lc::platform::width(rect) == 1920);
    CHECK(lc::platform::height(rect) == 1080);
    CHECK(lc::platform::contains(rect, {-1, 1079}));
}

TEST_CASE("physical rectangles use half-open right and bottom edges") {
    const lc::platform::PhysicalRect rect{0, 0, 2560, 1440};
    CHECK(lc::platform::contains(rect, {0, 0}));
    CHECK_FALSE(lc::platform::contains(rect, {2560, 0}));
    CHECK_FALSE(lc::platform::contains(rect, {0, 1440}));
}

TEST_CASE("inverted or empty physical rectangles are invalid") {
    CHECK_FALSE(lc::platform::isValid({0, 0, 0, 100}));
    CHECK_FALSE(lc::platform::isValid({50, 50, 49, 51}));
}
```

- [ ] **Step 2: 运行测试并确认 RED**

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
```

预期：编译失败，明确缺少 `platform/MonitorTypes.hpp` 或对应类型；不能因 CMake 路径拼写
错误失败。

- [ ] **Step 3: 实现最小基础类型**

接口固定为：

```cpp
namespace lc::platform {

struct MonitorId final {
    std::string value;
    auto operator<=>(const MonitorId&) const = default;
};

struct PhysicalPoint final {
    std::int32_t x;
    std::int32_t y;
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

std::int64_t width(PhysicalRect rect) noexcept;
std::int64_t height(PhysicalRect rect) noexcept;
bool isValid(PhysicalRect rect) noexcept;
bool contains(PhysicalRect rect, PhysicalPoint point) noexcept;

}  // namespace lc::platform
```

宽高使用 64 位中间结果，防止两个 32 位有符号桌面边界相减溢出。`contains()` 对无效矩形
返回 false。

- [ ] **Step 4: 运行 GREEN 与全量回归**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
ctest --preset windows-msvc-debug -R "physical rectangles" --output-on-failure
ctest --preset windows-msvc-debug --output-on-failure
```

预期：新增 3 个测试和既有测试全部通过。

- [ ] **Step 5: 建立中文进度文档并提交**

进度文档必须列出本计划、规格、执行分支/工作树、10 个任务表格、Task 1 RED/GREEN 命令与
结果。然后运行：

```powershell
git diff --check
git add src/platform src/CMakeLists.txt tests/platform tests/CMakeLists.txt `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: add physical monitor geometry types"
```

---

### Task 2: 固定 Per-Monitor V2 DPI 启动边界

**文件：**

- 新建：`src/platform/windows/DpiAwareness.hpp`
- 新建：`src/platform/windows/DpiAwareness.cpp`
- 新建：`resources/windows/LandscapeCutter.manifest`
- 新建：`resources/windows/LandscapeCutter.rc`
- 新建：`tests/platform/windows/DpiAwarenessTests.cpp`
- 修改：`src/main.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：当前进程由 manifest 或宿主预先建立的 DPI awareness context。
- 输出：`DpiSetupResult ensurePerMonitorV2() noexcept` 与
  `bool isPerMonitorV2() noexcept`。
- `main()` 必须在 `QApplication` 构造前完成并验证该调用。

- [ ] **Step 1: 写 DPI 进程 RED 测试**

创建独立测试目标 `landscapecutter_dpi_tests`，测试目标也嵌入同一 manifest：

```cpp
TEST_CASE("the process runs as Per-Monitor V2 before Qt exists") {
    const auto result = lc::platform::windows::ensurePerMonitorV2();
    CHECK(result.status != lc::platform::windows::DpiSetupStatus::Failed);
    CHECK(lc::platform::windows::isPerMonitorV2());
}
```

- [ ] **Step 2: 运行测试并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_dpi_tests
```

预期：因 DPI 类型或实现缺失而编译失败。

- [ ] **Step 3: 添加 manifest 与运行时验证**

manifest 必须同时声明 Windows 10 compatibility 和 `PerMonitorV2`：

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
    </windowsSettings>
  </application>
</assembly>
```

资源脚本使用：

```rc
#include <winuser.h>
1 RT_MANIFEST "LandscapeCutter.manifest"
```

运行时接口固定为：

```cpp
enum class DpiSetupStatus { Configured, AlreadyPerMonitorV2, Failed };

struct DpiSetupResult final {
    DpiSetupStatus status;
    unsigned long nativeError;
};

DpiSetupResult ensurePerMonitorV2() noexcept;
bool isPerMonitorV2() noexcept;
```

`SetProcessDpiAwarenessContext` 成功返回 `Configured`；若返回 `ERROR_ACCESS_DENIED`，必须用
`AreDpiAwarenessContextsEqual` 验证当前上下文为 Per-Monitor V2 后返回
`AlreadyPerMonitorV2`；其余情况返回 `Failed`。

- [ ] **Step 4: 把 DPI 检查放到 QApplication 之前**

`main()` 的顺序必须是收集参数、初始化 C++/WinRT apartment，然后立即验证 DPI，
再构造 `QApplication`。失败时使用原生
`MessageBoxW` 报告坐标模式不可用并返回非零；失败路径不得先创建 Qt 窗口。

- [ ] **Step 5: 运行 GREEN、检查 manifest 并提交**

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R "Per-Monitor V2|app_process_smoke" --output-on-failure
& mt.exe -nologo `
  '-inputresource:out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe;#1' `
  "-out:$env:TEMP\LandscapeCutter.manifest.xml"
Select-String -Path $env:TEMP\LandscapeCutter.manifest.xml -Pattern "PerMonitorV2"
git diff --check
```

把 RED/GREEN、提取出的 manifest 证据写入进度文档，提交：

```powershell
git add src/main.cpp src/platform/windows resources/windows src/CMakeLists.txt `
  tests/platform/windows tests/CMakeLists.txt docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: enforce Per-Monitor V2 awareness"
```

---

### Task 3: 建立原生消息窗口与全局快捷键服务

**文件：**

- 新建：`src/platform/windows/NativeMessageWindow.hpp`
- 新建：`src/platform/windows/NativeMessageWindow.cpp`
- 新建：`src/platform/windows/GlobalHotkeyService.hpp`
- 新建：`src/platform/windows/GlobalHotkeyService.cpp`
- 新建：`tests/platform/windows/NativeMessageWindowTests.cpp`
- 新建：`tests/platform/windows/GlobalHotkeyServiceTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：Qt 主线程、Windows 消息、`HotkeyBinding`。
- 输出：类型化的 `hotkeyMessage(int)`、`displayConfigurationChanged()` 和
  `GlobalHotkeyService::activated()` Qt 信号。
- 正式快捷键固定为 ID `0x4C43`、`MOD_NOREPEAT`、`VK_F2`；测试使用 `VK_F24`。

- [ ] **Step 1: 写不可见窗口与消息转发 RED 测试**

使用独立的 `QCoreApplication` 测试目标，覆盖：

```cpp
TEST_CASE("native message window is an invisible top-level tool window");
TEST_CASE("native message window forwards hotkey ids");
TEST_CASE("native message window forwards display change broadcasts");
```

断言 `IsWindow(handle)` 为真、`IsWindowVisible(handle)` 为假、父窗口不是 `HWND_MESSAGE`、
扩展样式包含 `WS_EX_TOOLWINDOW`，并通过 `PostMessageW` 验证类型化信号。测试不能创建用户可见
窗口。

- [ ] **Step 2: 运行测试并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_platform_message_tests
```

预期：缺少 `NativeMessageWindow` 和 `GlobalHotkeyService` 导致编译失败。

- [ ] **Step 3: 实现 NativeMessageWindow**

接口固定为：

```cpp
class NativeMessageWindow final : public QObject {
    Q_OBJECT
public:
    explicit NativeMessageWindow(QObject* parent = nullptr);
    ~NativeMessageWindow() override;
    bool create();
    HWND handle() const noexcept;

signals:
    void hotkeyMessage(int id);
    void displayConfigurationChanged();
};
```

使用固定类名 `LandscapeCutter.NativeMessageWindow`，创建不可见的 `WS_POPUP`、
`WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE` 顶层窗口。WndProc 只转发 `WM_HOTKEY`、
`WM_DISPLAYCHANGE`、`WM_DEVICECHANGE` 和 `WM_CLOSE`；`WM_CLOSE` 调用 Qt 退出请求。析构时在
同一线程销毁 HWND 并注销窗口类。

- [ ] **Step 4: 实现 GlobalHotkeyService**

```cpp
struct HotkeyBinding final {
    int id;
    unsigned int modifiers;
    unsigned int virtualKey;
};

enum class HotkeyRegistrationStatus { Registered, Conflict, Failed };

class GlobalHotkeyService final : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkeyService(NativeMessageWindow& window, QObject* parent = nullptr);
    ~GlobalHotkeyService() override;
    HotkeyRegistrationStatus registerBinding(HotkeyBinding binding);
    void unregister() noexcept;
    bool isRegistered() const noexcept;

signals:
    void activated();
};
```

重复注册先注销旧 binding。`RegisterHotKey` 返回 `ERROR_HOTKEY_ALREADY_REGISTERED` 映射为
`Conflict`，其他错误映射为 `Failed`。析构必须调用 `UnregisterHotKey`。

- [ ] **Step 5: 增加快捷键 GREEN 测试**

用 `VK_F24` 验证成功注册；第二个窗口注册同一组合必须得到 `Conflict`；向第一个 HWND 发送
匹配的 `WM_HOTKEY` 后只触发一次 `activated()`；注销后第二个服务可以注册该组合。

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_platform_message_tests
ctest --preset windows-msvc-debug -R "native message|global hotkey" --output-on-failure
```

- [ ] **Step 6: 更新进度并提交**

```powershell
git diff --check
git add src/platform/windows src/CMakeLists.txt tests/platform/windows tests/CMakeLists.txt `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: add native messages and global hotkeys"
```

---

### Task 4: 实现可通知现有进程的单实例协议

**文件：**

- 新建：`src/platform/windows/SingleInstanceCoordinator.hpp`
- 新建：`src/platform/windows/SingleInstanceCoordinator.cpp`
- 新建：`tests/platform/windows/SingleInstanceCoordinatorTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：正式固定命名空间或测试唯一后缀。
- 输出：`Primary`、`Secondary`、`Failed`，以及主实例的 `activationRequested()` 信号。
- 固定产品 GUID：`4F4E6D0D-8C33-4B79-984A-3E44F6A62D11`。

- [ ] **Step 1: 写单实例竞态 RED 测试**

```cpp
TEST_CASE("the first coordinator is primary and the second is secondary");
TEST_CASE("a secondary coordinator wakes the primary exactly once");
TEST_CASE("a new coordinator becomes primary after all handles close");
```

每个测试用 GUID 生成独立后缀。第二项必须先 `SetEvent`，再让主实例建立 notifier，证明信号
不会因 Qt 事件循环尚未启动而丢失。

- [ ] **Step 2: 运行并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_single_instance_tests
```

预期：缺少单实例类型导致编译失败。

- [ ] **Step 3: 实现 event-before-mutex 协议**

```cpp
enum class InstanceRole { Primary, Secondary, Failed };

struct InstanceAcquireResult final {
    InstanceRole role;
    unsigned long nativeError;
};

class SingleInstanceCoordinator final : public QObject {
    Q_OBJECT
public:
    explicit SingleInstanceCoordinator(std::wstring namespaceSuffix = {},
                                       QObject* parent = nullptr);
    ~SingleInstanceCoordinator() override;
    InstanceAcquireResult acquire();
    bool signalPrimary() noexcept;
    bool beginListening();
    InstanceRole role() const noexcept;

signals:
    void activationRequested();
};
```

正式对象名为：

```text
Local\LandscapeCutter.4F4E6D0D-8C33-4B79-984A-3E44F6A62D11.Activate
Local\LandscapeCutter.4F4E6D0D-8C33-4B79-984A-3E44F6A62D11.Instance
```

测试后缀追加在 GUID 与 `.Activate`/`.Instance` 之间。先创建自动复位 event，再创建并持有
mutex。主实例在 `QCoreApplication` 存在后用 `QWinEventNotifier` 监听 event；次实例只
`SetEvent`。使用 WIL unique handle，析构释放所有句柄。

- [ ] **Step 4: 运行 GREEN 与句柄回归**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_single_instance_tests
ctest --preset windows-msvc-debug -R "coordinator|secondary" --output-on-failure
```

测试结束后重新创建同名对象必须得到 `Primary`，证明没有残留 handle。

- [ ] **Step 5: 更新进度并提交**

```powershell
git diff --check
git add src/platform/windows src/CMakeLists.txt tests/platform/windows tests/CMakeLists.txt `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: add single-instance activation protocol"
```

---

### Task 5: 建立可刷新且可测试的显示器目录

**文件：**

- 新建：`src/platform/windows/MonitorDescriptor.hpp`
- 新建：`src/platform/windows/DisplayTopologySource.hpp`
- 新建：`src/platform/windows/WindowsDisplayTopologySource.hpp`
- 新建：`src/platform/windows/WindowsDisplayTopologySource.cpp`
- 新建：`src/platform/windows/DisplayCatalog.hpp`
- 新建：`src/platform/windows/DisplayCatalog.cpp`
- 新建：`tests/platform/windows/DisplayCatalogTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：`IDisplayTopologySource::read()` 返回的活动 monitor 和 display path 快照。
- 输出：全有或全无的显示器目录、monotonic generation、按 `HMONITOR` 或物理点查询。
- `MonitorId` 使用确定性 UTF-8：适配器 LUID、target ID、monitor device path。

- [ ] **Step 1: 写目录 RED 测试**

使用内存中的 fake topology source 覆盖：

```cpp
TEST_CASE("display refresh joins monitors by exact GDI device name");
TEST_CASE("display refresh rejects duplicate or missing path mappings");
TEST_CASE("failed refresh keeps the previous snapshot but marks it unhealthy");
TEST_CASE("successful refresh increments generation and stamps every monitor");
TEST_CASE("monitor lookup handles negative coordinates and half-open edges");
TEST_CASE("monitor ids are deterministic UTF-8 values");
```

期待值必须使用手写的矩形、LUID、target ID 和 UTF-8 ID，不调用生产 ID builder 生成期望值。

- [ ] **Step 2: 运行并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
```

预期：缺少显示器目录接口导致编译失败。

- [ ] **Step 3: 定义 topology source 与 descriptor**

```cpp
struct NativeMonitorRecord final {
    HMONITOR handle;
    std::wstring gdiDeviceName;
    platform::PhysicalRect desktopRect;
    platform::PhysicalRect workRect;
    std::uint32_t dpiX;
    std::uint32_t dpiY;
    bool primary;
};

struct DisplayPathRecord final {
    std::wstring gdiDeviceName;
    std::wstring friendlyName;
    std::wstring monitorDevicePath;
    LUID adapterId;
    std::uint32_t targetId;
    bool hdrEnabled;
};

struct TopologySnapshot final {
    std::vector<NativeMonitorRecord> monitors;
    std::vector<DisplayPathRecord> paths;
};

using TopologyReadResult = std::variant<TopologySnapshot, DisplayError>;

class IDisplayTopologySource {
public:
    virtual ~IDisplayTopologySource() = default;
    virtual TopologyReadResult read() = 0;
};
```

`MonitorDescriptor` 字段必须与规格第 5.1 节完全一致。

- [ ] **Step 4: 实现 WindowsDisplayTopologySource**

组合 `EnumDisplayMonitors`、`GetMonitorInfoW`、
`QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS)`、`DisplayConfigGetDeviceInfo` 和
`GetDpiForMonitor(MDT_EFFECTIVE_DPI)`。所有调用检查返回码、循环处理
`ERROR_INSUFFICIENT_BUFFER`，DPI 必须大于零。HDR 使用活动 target 的 advanced-color enabled
状态，不把“支持 HDR”误当作“当前启用 HDR”。

- [ ] **Step 5: 实现 DisplayCatalog**

```cpp
class DisplayCatalog final {
public:
    using RefreshResult = std::variant<std::vector<MonitorDescriptor>, DisplayError>;
    explicit DisplayCatalog(IDisplayTopologySource& source);
    RefreshResult refresh();
    std::optional<MonitorDescriptor> findByNativeHandle(HMONITOR handle) const;
    std::optional<MonitorDescriptor> monitorContaining(platform::PhysicalPoint point) const;
    std::optional<MonitorDescriptor> monitorFromPoint(POINT point) const;
    std::uint64_t generation() const noexcept;
    bool healthy() const noexcept;
};
```

刷新先在局部变量中完成所有映射和验证，成功后一次性发布并将 generation 加一；失败只把
`healthy` 设为 false，不部分覆盖旧数据。`monitorFromPoint(POINT)` 保留规格公开契约，
内部调用 Win32 `MonitorFromPoint(..., MONITOR_DEFAULTTONEAREST)` 后复用
`findByNativeHandle()`；`monitorContaining()` 供纯坐标边界测试与内部查询使用。

- [ ] **Step 6: 运行 GREEN、实际枚举诊断与提交**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
ctest --preset windows-msvc-debug -R "display refresh|monitor lookup|monitor ids" `
  --output-on-failure
git diff --check
```

实际 Windows source 的桌面枚举只输出显示器数量、物理矩形、DPI 和 HDR 布尔值，不输出画面。
将结果和环境具备的显示器数量写入进度文档，然后提交：

```powershell
git add src/platform src/CMakeLists.txt tests/platform tests/CMakeLists.txt `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: add physical display catalog"
```

---

### Task 6: 建立 D3D11 设备、WARP 降级与自有纹理

**文件：**

- 新建：`src/graphics/d3d11/D3d11DeviceFactory.hpp`
- 新建：`src/graphics/d3d11/D3d11DeviceFactory.cpp`
- 新建：`src/graphics/d3d11/D3d11DeviceManager.hpp`
- 新建：`src/graphics/d3d11/D3d11DeviceManager.cpp`
- 新建：`src/graphics/d3d11/TextureCopy.hpp`
- 新建：`src/graphics/d3d11/TextureCopy.cpp`
- 新建：`tests/graphics/d3d11/D3d11DeviceManagerTests.cpp`
- 新建：`tests/graphics/d3d11/TextureCopyTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：硬件或 WARP driver kind、源 `ID3D11Texture2D`。
- 输出：全进程当前设备快照、设备 generation、自有纹理和受控 readback buffer。
- 链接：`d3d11`、`dxgi`、`windowsapp`、`dxguid`。

- [ ] **Step 1: 写设备降级和代际 RED 测试**

通过 fake `ID3d11DeviceFactory` 覆盖：

```cpp
TEST_CASE("device manager prefers a hardware device");
TEST_CASE("device manager falls back to WARP after hardware failure");
TEST_CASE("device manager reports failure when both drivers fail");
TEST_CASE("each successful rebuild increments device generation once");
```

设备 manager 只负责一次 `rebuild()` 语义与 generation；“每个捕获请求最多恢复一次”
由 Task 8 的 `CaptureCoordinator` 状态测试覆盖。

- [ ] **Step 2: 写纹理所有权 RED 测试**

使用真实 WARP 设备创建 2×2 BGRA8 纹理，写入固定 16 字节图案，调用计划中的
`TextureCopy::copyOwned()`，释放源纹理，再 readback 自有纹理并断言尺寸、row pitch 下的四个
像素和格式仍正确。另加 RGBA16F 描述与复制测试。

- [ ] **Step 3: 运行并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_graphics_tests
```

预期：缺少 D3D11 管理和纹理复制类型导致编译失败。

- [ ] **Step 4: 实现设备工厂与 manager**

```cpp
enum class D3dDriverKind { Hardware, Warp };
enum class D3dErrorCode { CreationFailed, DeviceLost, InvalidTexture, CopyFailed, ReadbackFailed };

struct D3dDeviceBundle final {
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> immediateContext;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice{nullptr};
    D3D_FEATURE_LEVEL featureLevel{};
    D3dDriverKind driverKind{};
};

using D3dCreateResult = std::variant<D3dDeviceBundle, D3dError>;

class ID3d11DeviceFactory {
public:
    virtual ~ID3d11DeviceFactory() = default;
    virtual D3dCreateResult create(D3dDriverKind kind) = 0;
};

class ID3d11DeviceRecovery {
public:
    virtual ~ID3d11DeviceRecovery() = default;
    virtual bool rebuild() = 0;
    virtual std::uint64_t generation() const noexcept = 0;
};
```

`D3d11DeviceManager` 实现 recovery 接口，`initialize()`/`rebuild()` 都按 Hardware→WARP
顺序调用 factory。创建 flags 始终包含 `D3D11_CREATE_DEVICE_BGRA_SUPPORT`，feature level
只接受 11.1 和 11.0。Debug layer 缺失时重试无 debug flag，不把 Graphics Tools 变成运行
依赖。成功后创建 C++/WinRT `IDirect3DDevice` 并原子发布新 bundle/generation。

- [ ] **Step 5: 实现 TextureCopy 串行边界**

```cpp
struct TextureReadback final {
    platform::PixelSize size;
    DXGI_FORMAT format;
    std::size_t rowPitch;
    std::vector<std::byte> bytes;
};

class TextureCopy final {
public:
    explicit TextureCopy(D3d11DeviceManager& manager);
    std::variant<winrt::com_ptr<ID3D11Texture2D>, D3dError>
    copyOwned(ID3D11Texture2D& source);
    std::variant<TextureReadback, D3dError> readback(ID3D11Texture2D& source);
};
```

两个操作都经 manager 的同一 mutex 使用立即上下文。自有纹理保留尺寸和格式，去除不适合
长期所有权的 staging/cpu-access flags；readback 正确处理 row pitch，不假设紧密排列。

- [ ] **Step 6: 运行 GREEN 与真实 WARP 测试**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_graphics_tests
ctest --preset windows-msvc-debug -R "device manager|WARP|texture" --output-on-failure
```

真实硬件创建结果写入进度文档；CI 没有硬件适配器时可以记录 WARP，但真实开发机验收必须
证明 Hardware 被优先选择。

- [ ] **Step 7: 更新进度并提交**

```powershell
git diff --check
git add src/graphics src/CMakeLists.txt tests/graphics tests/CMakeLists.txt `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: add shared D3D11 graphics device"
```

---

### Task 7: 实现 WGC 显示器单帧捕获服务

**文件：**

- 新建：`src/capture/CaptureFrame.hpp`
- 新建：`src/capture/CaptureResult.hpp`
- 新建：`src/capture/MonitorCaptureService.hpp`
- 新建：`src/capture/windows/GraphicsCaptureInterop.hpp`
- 新建：`src/capture/windows/GraphicsCaptureInterop.cpp`
- 新建：`src/capture/windows/OneShotCaptureState.hpp`
- 新建：`src/capture/windows/MonitorCaptureService.hpp`
- 新建：`src/capture/windows/MonitorCaptureService.cpp`
- 新建：`tests/capture/windows/OneShotCaptureStateTests.cpp`
- 新建：`tests/capture/windows/GraphicsCaptureInteropTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：`MonitorCaptureRequest`，只包含完整 monitor descriptor 和 2000 ms timeout；
  display generation 来自 `monitor.catalogGeneration`，device generation 由捕获服务从设备 manager 快照。
- 输出：恰好一次 `CaptureCompletion(CaptureResult)`；成功值拥有独立 GPU 纹理。
- 只在 `capture/windows` 内出现 `GraphicsCaptureItem`、frame pool、session 和 WinRT frame。

- [ ] **Step 1: 写一次性完成门 RED 测试**

```cpp
TEST_CASE("the first valid frame wins completion exactly once");
TEST_CASE("timeout wins when no frame arrives");
TEST_CASE("cancel suppresses UI completion during shutdown");
TEST_CASE("late frames cannot complete after timeout");
TEST_CASE("empty frames fail completion exactly once");
TEST_CASE("content size and texture description mismatches are rejected");
```

测试用可控 fake frame source 触发 frame/timeout/cancel 的不同顺序，断言真实
`OneShotCaptureState` 的状态和 completion 次数，不只断言 fake 调用次数。

- [ ] **Step 2: 运行并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_capture_tests
```

预期：缺少捕获类型和状态机导致编译失败。

- [ ] **Step 3: 实现标准捕获类型**

按规格第 5.2–5.4 节实现：

```cpp
enum class CapturePixelFormat { Bgra8Unorm, Rgba16Float };

struct CaptureFrame final {
    winrt::com_ptr<ID3D11Texture2D> texture;
    platform::PixelSize size;
    CapturePixelFormat pixelFormat;
    std::optional<std::chrono::nanoseconds> systemRelativeTime;
    platform::MonitorId sourceMonitor;
    std::uint64_t displayGeneration;
    std::uint64_t deviceGeneration;
};

struct MonitorCaptureRequest final {
    platform::windows::MonitorDescriptor monitor;
    std::chrono::milliseconds timeout{2000};
};

using CaptureResult = std::variant<CaptureFrame, CaptureError>;
using CaptureCompletion = std::function<void(CaptureResult)>;

class IMonitorCaptureService {
public:
    virtual ~IMonitorCaptureService() = default;
    virtual void captureOnce(MonitorCaptureRequest request, CaptureCompletion completion) = 0;
    virtual void cancel() noexcept = 0;
};
```

`CaptureFrame` 显式可移动、不可复制。错误枚举与规格完全一致。

- [ ] **Step 4: 实现 WinRT/DXGI interop**

`GraphicsCaptureInterop` 只提供两个窄接口：

```cpp
winrt::Windows::Graphics::Capture::GraphicsCaptureItem
createItemForMonitor(HMONITOR monitor);

winrt::com_ptr<ID3D11Texture2D> textureFromSurface(
    const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& surface);
```

分别使用 `IGraphicsCaptureItemInterop::CreateForMonitor` 和
`IDirect3DDxgiInterfaceAccess::GetInterface`；HRESULT 失败映射为 `CaptureError`，不能把异常穿过
Qt 事件边界。

- [ ] **Step 5: 实现 MonitorCaptureService**

服务在 Qt 主线程接收请求，验证 `GraphicsCaptureSession::IsSupported()`，根据
`monitor.hdrEnabled` 选择 BGRA8 或 RGBA16F，以 buffer count 2 调用
`Direct3D11CaptureFramePool::CreateFreeThreaded`。`FrameArrived` 在线程池中取得一次处理权，
验证 `ContentSize`/texture description，通过 `TextureCopy::copyOwned()` 建立自有纹理，再用
`QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 把结果送回主线程统一关闭 session、frame
pool 和 timer。
服务必须先撤销 token 并关闭 session/frame pool，再调用 completion，使业务层收到结果时
已经不依赖 WGC 会话寿命。

timeout、frame、cancel、析构共享同一个原子完成门。析构路径只清理，不向已销毁 UI 回调。

- [ ] **Step 6: 运行 GREEN、资源循环与提交**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_capture_tests
ctest --preset windows-msvc-debug -R "valid frame|timeout|cancel|late frames|interop" `
  --output-on-failure
git diff --check
```

循环建立/取消 fake capture 100 次，结束时 completion、timer 和活动 callback 计数必须归零。
记录到进度文档并提交：

```powershell
git add src/capture src/CMakeLists.txt tests/capture tests/CMakeLists.txt `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: capture one monitor frame with WGC"
```

---

### Task 8: 实现捕获业务状态与设备恢复

**文件：**

- 新建：`src/app/CaptureCoordinator.hpp`
- 新建：`src/app/CaptureCoordinator.cpp`
- 新建：`tests/app/CaptureCoordinatorTests.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 输入：显示器选择函数、`IMonitorCaptureService`、`ID3d11DeviceRecovery`。
- 输出：`CaptureNotice`、`Idle/Capturing` 状态和只读最近成功帧。
- 设备丢失只重建并重试一次；其他失败不自动重试。

- [ ] **Step 1: 写协调器 RED 测试**

```cpp
TEST_CASE("a capture request targets the monitor under the cursor");
TEST_CASE("a second request while capturing is rejected without queuing");
TEST_CASE("success replaces the latest frame and returns to idle");
TEST_CASE("failure preserves the latest valid frame");
TEST_CASE("stale display and device generations are rejected");
TEST_CASE("device loss rebuilds and retries exactly once");
TEST_CASE("a second device loss stops retrying and reports failure");
TEST_CASE("every capture error code maps to a structured notice");
TEST_CASE("shutdown cancellation does not emit a user notice");
```

fake capture service保存真实 completion 并由测试决定何时返回；断言 coordinator 的状态、
最新帧和结构化 notice，不以 fake 调用本身作为唯一断言。

- [ ] **Step 2: 运行并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
```

预期：缺少 `CaptureCoordinator` 导致编译失败。

- [ ] **Step 3: 定义结构化通知和 availability**

```cpp
enum class CaptureState { Idle, Capturing };
enum class CaptureAvailability { Available, Unsupported, DisplayUnavailable, DeviceUnavailable };
enum class CaptureNoticeCode {
    Success,
    Busy,
    HotkeyConflict,
    Unsupported,
    MonitorUnavailable,
    AccessDenied,
    DisplayUnavailable,
    Timeout,
    InvalidFrame,
    DisplayChanged,
    DeviceUnavailable,
    DeviceRecoveryFailed,
    CaptureCancelled,
    InternalFailure,
};

struct CaptureNotice final {
    CaptureNoticeCode code;
    std::optional<platform::MonitorId> monitor;
    std::wstring displayName;
    std::optional<platform::PixelSize> size;
    std::optional<capture::CapturePixelFormat> pixelFormat;
    std::chrono::milliseconds elapsed;
};
```

通知结构不保存最终中文文案；`displayName` 只保存当前 monitor descriptor 的友好名。
`CaptureCoordinator` 必须将规格中每个 `CaptureErrorCode` 显式映射到上述结构化类别；
`AppController` 在下一任务负责格式化显示。

- [ ] **Step 4: 实现 CaptureCoordinator**

```cpp
using MonitorSelector =
    std::function<std::optional<platform::windows::MonitorDescriptor>()>;

class CaptureCoordinator final : public QObject {
    Q_OBJECT
public:
    CaptureCoordinator(capture::IMonitorCaptureService& captureService,
                       graphics::d3d11::ID3d11DeviceRecovery& deviceRecovery,
                       MonitorSelector monitorSelector,
                       QObject* parent = nullptr);
    void requestCapture();
    void setAvailability(CaptureAvailability availability);
    void shutdown() noexcept;
    CaptureState state() const noexcept;
    const std::optional<capture::CaptureFrame>& latestFrame() const noexcept;

signals:
    void noticeReady(const CaptureNotice& notice);
    void availabilityChanged(CaptureAvailability availability);
};
```

请求时记录 steady-clock 起点、display generation、device generation 和 retry count。所有
completion 先检查 shutdown 与代际，再修改状态。`DeviceLost` 只在 retry count 为 0 时调用
`rebuild()` 并重发同一 monitor 请求；重建后更新 device generation。设备丢失时释放旧设备
代际的 `latestFrame`。

- [ ] **Step 5: 运行 GREEN 与全量逻辑回归**

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
ctest --preset windows-msvc-debug -R "capture request|latest frame|device loss|shutdown" `
  --output-on-failure
ctest --preset windows-msvc-debug --output-on-failure
```

- [ ] **Step 6: 更新进度并提交**

```powershell
git diff --check
git add src/app src/CMakeLists.txt tests/app tests/CMakeLists.txt `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: coordinate one-shot monitor capture"
```

---

### Task 9: 接入托盘、F2、显示器刷新与单实例启动

**文件：**

- 修改：`src/app/AppController.hpp`
- 修改：`src/app/AppController.cpp`
- 修改：`src/main.cpp`
- 修改：`src/CMakeLists.txt`
- 修改：`tests/app/AppControllerTests.cpp`
- 新建：`tests/scripts/AppProcessBehaviorTests.ps1`
- 新建：`tests/helpers/HotkeyOccupier.cpp`
- 修改：`tests/CMakeLists.txt`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- `main()` 是唯一 composition root，按规格启动顺序构造具体服务。
- `AppController` 只管理托盘 action 和显示通知，通过 Qt signal 调用 coordinator。
- 产品原生窗口类固定为 `LandscapeCutter.NativeMessageWindow`，用于进程测试安全退出。

- [ ] **Step 1: 写托盘行为 RED 测试**

扩展 offscreen AppController 测试：

```cpp
TEST_CASE("the tray menu exposes capture before quit");
TEST_CASE("capture availability enables and disables the capture action");
TEST_CASE("triggering the tray capture action emits one capture request");
TEST_CASE("structured success notices include monitor size and pixel format");
TEST_CASE("a hotkey conflict keeps tray capture enabled and reports the conflict");
```

给 capture action 设置稳定 object name `captureCurrentMonitorAction`，测试通过 QObject 查询真实
action 并触发；不添加只供测试调用的生产 accessor。文案测试只断言显示器名、尺寸和格式均被
包含，不锁定整句标点。

- [ ] **Step 2: 写进程启动 RED 测试**

`AppProcessBehaviorTests.ps1` 在确认没有真实 LandscapeCutter 实例后：

1. 启动第一个正式 `LandscapeCutter.exe`；
2. 等待固定原生窗口类出现；
3. 启动第二个正式进程；
4. 断言第二进程在 3 秒内以 0 退出而第一进程保持运行；
5. 向第一个进程的原生窗口发送 `WM_CLOSE`；
6. 断言第一进程退出且窗口、进程和产品命名对象消失。

同一脚本再启动 `landscapecutter_hotkey_occupier.exe`，由 helper 在自己的隐藏窗口上占用
`F2` 并用唯一命名 event 通知已就绪。随后启动产品，断言产品在 3 秒后仍运行；
`AppController` 单元测试同时断言 conflict 状态下菜单捕获项仍可用。脚本必须通过
stop event 让 helper 正常退出，不得用它占用或关闭用户进程。

脚本若检测到测试前已有真实 LandscapeCutter 实例，必须以 CTest skip return code 125 退出，
不能关闭用户进程。

- [ ] **Step 3: 运行并确认 RED**

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R "tray menu|capture availability|app_process_behavior" `
  --output-on-failure
```

预期：菜单用例缺少 action；进程用例证明第二个实例仍可进入现有启动路径或缺少原生窗口。

- [ ] **Step 4: 扩展 AppController**

`AppController` 增加 `QAction captureAction_{tr("捕获当前显示器单帧"), this}`，object name 固定
为 `captureCurrentMonitorAction`，位于“退出”之前。新增：

```cpp
void setCaptureEnabled(bool enabled);
void showCaptureNotice(const CaptureNotice& notice);
void showAlreadyRunning();
void showHotkeyConflict();

signals:
    void captureRequested();
```

`showCaptureNotice()` 把 Success 映射为“已捕获 <友好名>：<宽> × <高>（SDR BGRA8）”或
“（HDR RGBA16F）”；其他 code 使用简短可操作文案。托盘仍不可用时不致命，Qt 延迟注册行为
保持不变。

- [ ] **Step 5: 按固定顺序装配 main**

```text
winrt::init_apartment(single_threaded)
→ ensurePerMonitorV2
→ SingleInstanceCoordinator::acquire
→ Secondary: signalPrimary 并返回 0
→ QApplication
→ AppController::start
→ NativeMessageWindow::create
→ SingleInstanceCoordinator::beginListening
→ WindowsDisplayTopologySource + DisplayCatalog::refresh
→ WindowsD3d11DeviceFactory + D3d11DeviceManager::initialize
→ TextureCopy + MonitorCaptureService + CaptureCoordinator
→ GlobalHotkeyService 注册 {0x4C43, MOD_NOREPEAT, VK_F2}
→ Qt signal 连接
→ application.exec
```

捕获能力不可用时 `setCaptureEnabled(false)` 且不注册 `F2`。只有热键 conflict 时保持菜单
enabled 并通知。Hardware 失败但 WARP 成功时通知兼容模式。显示器变化信号以 100 ms
single-shot `QTimer` 防抖刷新目录；刷新失败时禁用捕获，成功后恢复。

退出连接必须先调用 coordinator shutdown 和 hotkey unregister，再允许 Qt 对象逆序析构。

- [ ] **Step 6: 运行 GREEN、完整回归与提交**

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R "tray menu|capture availability|app_process_behavior|app_process_smoke" `
  --output-on-failure
ctest --preset windows-msvc-debug --output-on-failure
git diff --check
```

进度文档记录单实例、菜单、F2 注册状态和非交互回归结果，然后提交：

```powershell
git add src/main.cpp src/app src/CMakeLists.txt tests/app tests/scripts `
  tests/CMakeLists.txt docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "feat: wire Windows capture into the tray app"
```

---

### Task 10: 建立真实桌面捕获验收并完成里程碑

**文件：**

- 新建：`tests/integration/windows/DesktopCaptureIntegrationTests.cpp`
- 修改：`tests/CMakeLists.txt`
- 修改：`CMakePresets.json`
- 修改：`README.md`
- 修改：`docs/superpowers/progress/2026-09-03-milestone-1-progress.md`

**接口：**

- 默认 preset 排除 label `desktop-integration`。
- 新 preset `windows-msvc-debug-desktop` 只在真实交互桌面运行 desktop integration tests。
- 桌面测试逐一捕获当前所有活动显示器，不写文件、不输出像素。

- [ ] **Step 1: 写真实桌面验收测试**

```cpp
TEST_CASE("every active monitor produces one owned GPU frame") {
    DesktopCaptureFixture fixture;
    const auto monitors = fixture.refreshMonitors();
    REQUIRE_FALSE(monitors.empty());

    for (const auto& monitor : monitors) {
        CAPTURE(monitor.id.value, monitor.desktopRect.left, monitor.desktopRect.top);
        auto frame = fixture.captureOnce(monitor, std::chrono::milliseconds{2000});
        REQUIRE(frame.texture);

        D3D11_TEXTURE2D_DESC desc{};
        frame.texture->GetDesc(&desc);
        CHECK(desc.Width == frame.size.width);
        CHECK(desc.Height == frame.size.height);
        CHECK(frame.sourceMonitor == monitor.id);
        CHECK(frame.displayGeneration == monitor.catalogGeneration);
        CHECK(frame.deviceGeneration == fixture.deviceGeneration());

        const auto expectedFormat = monitor.hdrEnabled
            ? lc::capture::CapturePixelFormat::Rgba16Float
            : lc::capture::CapturePixelFormat::Bgra8Unorm;
        const auto expectedDxgiFormat = monitor.hdrEnabled
            ? DXGI_FORMAT_R16G16B16A16_FLOAT
            : DXGI_FORMAT_B8G8R8A8_UNORM;
        CHECK(frame.pixelFormat == expectedFormat);
        CHECK(desc.Format == expectedDxgiFormat);
    }
}

TEST_CASE("captured textures survive WGC session shutdown and controlled readback") {
    DesktopCaptureFixture fixture;
    const auto monitors = fixture.refreshMonitors();
    REQUIRE_FALSE(monitors.empty());

    for (const auto& monitor : monitors) {
        auto frame = fixture.captureOnce(monitor, std::chrono::milliseconds{2000});
        auto readbackResult = fixture.readback(frame.texture);
        REQUIRE(std::holds_alternative<lc::graphics::d3d11::TextureReadback>(readbackResult));

        const auto& readback =
            std::get<lc::graphics::d3d11::TextureReadback>(readbackResult);
        CHECK(readback.size.width == frame.size.width);
        CHECK(readback.size.height == frame.size.height);
        CHECK(readback.rowPitch > 0);
        CHECK(readback.bytes.size() ==
              readback.rowPitch * static_cast<std::size_t>(readback.size.height));
        CHECK(readback.format == (monitor.hdrEnabled
            ? DXGI_FORMAT_R16G16B16A16_FLOAT
            : DXGI_FORMAT_B8G8R8A8_UNORM));
    }
}
```

同文件实现 `DesktopCaptureFixture`：按 Task 9 的相同顺序构造 Windows topology source、
catalog、D3D11 manager、texture copy 和 monitor capture service。`refreshMonitors()` 必须要求
`DisplayCatalog::refresh()` 返回完整非空目录；`captureOnce()` 用 single-shot `QTimer` 和
`QEventLoop` 实现 2500 ms 外层上限，超时时先 `cancel()` 再失败，回调只接受
`CaptureFrame`并按值移动返回。`readback()` 只返回 `TextureReadback`，测试不打印
`bytes`。WGC `ContentSize` 与原始 texture desc 的不匹配由 Task 7 的 fake-frame 测试
显式注入并验证拒绝；公开 `CaptureFrame` 不重复保存一份 ContentSize 调试字段。
没有活动显示器时失败，不得静默通过。

- [ ] **Step 2: 配置独立 desktop test preset 并记录首次结果**

`tests/CMakeLists.txt` 给测试设置 `LABELS desktop-integration`。默认 preset 添加 label exclude，
桌面 preset 添加 label include。运行：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target landscapecutter_desktop_capture_tests
ctest --preset windows-msvc-debug-desktop --output-on-failure
```

这是对 Tasks 1–9 已经 RED/GREEN 建立的生产行为做真实桌面验收，不为了仪式化 RED 故意破坏实现。
首次执行若失败，必须记录具体产品缺口并在 Step 3 修复；如果命令运行在非 Default/非交互桌面，
记录环境错误并改用受控 Default 桌面运行，不能把无桌面当作产品失败。

- [ ] **Step 3: 完成真实捕获测试的最小支持**

只补测试暴露出的生产缺口：异步有界等待、每显示器独立请求、session 关闭后的自有纹理
readback。不得添加截图保存、剪贴板或预览窗口。

- [ ] **Step 4: 运行自动与桌面 GREEN**

```powershell
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
ctest --preset windows-msvc-debug-desktop --output-on-failure
```

记录当前显示器数量、每个显示器的物理尺寸、SDR/HDR 格式和触发到自有帧完成耗时。不得记录
或落盘像素。常规桌面 150 ms 是性能目标；超出时记录实测和分析，不能伪造通过。

- [ ] **Step 5: 执行人工验收矩阵**

在 `WinSta0\Default` 正式启动产品并逐项确认：

```text
[ ] 鼠标放到每个已连接显示器，F2 通知的显示器与物理尺寸正确
[ ] 托盘“捕获当前显示器单帧”进入同一流程
[ ] 快速重复 F2 时只有一个活动请求，应用保持响应
[ ] 外部 helper 占用 F2 后，启动提示冲突且菜单捕获仍可用
[ ] 第二次启动自行退出，现有实例显示“已在运行”
[ ] 正常退出后无托盘残影、产品进程、原生窗口或活动捕获回调
```

用户可见结果由用户确认；控制器同时检查进程、原生窗口和相关 handle 生命周期。机器不具备
多屏、混合 DPI 或 HDR 时写“环境不具备”，对应逻辑测试仍须通过。

- [ ] **Step 6: 更新 README 和最终进度**

README 把里程碑 1 标为完成，说明 `F2` 当前只捕获内存单帧并通知尺寸，明确文件保存和
剪贴板属于里程碑 2；更新测试命令，不能把未具备的硬件矩阵写成已通过。

进度文档填写每个任务提交、审查结果、自动测试、桌面测试、人工验收和环境未覆盖项；只有
上述退出条件全部满足时状态才写“里程碑 1 已完成”。

- [ ] **Step 7: 从干净构建目录执行最终验证**

先解析并确认删除目标严格位于仓库 `out/build/windows-msvc-debug`，再删除该构建子目录。随后：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
ctest --preset windows-msvc-debug-desktop --output-on-failure
git diff --check
git status --short
```

同时验证 `.tools/vcpkg` exact tag、annotated tag object、peeled commit、HEAD、tracked status 和
特殊索引位仍符合里程碑 0 固定值。

- [ ] **Step 8: 提交里程碑完成记录**

```powershell
git add tests/integration tests/CMakeLists.txt CMakePresets.json README.md `
  docs/superpowers/progress/2026-09-03-milestone-1-progress.md
git commit -m "test: verify milestone 1 desktop capture"
```

实施者报告必须列出测试总数、真实显示器数量、每个未具备环境项和所有验收证据。最终分支
审查与控制器独立验证通过后，再由用户决定本地合并、创建 Pull Request 或保留分支。
