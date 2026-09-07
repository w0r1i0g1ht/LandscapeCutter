# 里程碑 1 执行进度

- 实施计划：[里程碑 1：Windows 图形基础实施计划](../plans/2026-09-03-milestone-1-windows-graphics-foundation.md)
- 规格：[里程碑 1：Windows 图形基础详细设计](../specs/2026-09-03-milestone-1-windows-graphics-foundation-design.md)
- 执行分支：`codex/milestone-1-windows-graphics-foundation`
- 工作树：`D:\Projects\LandscapeCutter\.worktrees\milestone-1-windows-graphics-foundation`
- 当前任务：Tasks 1–6 已完成并通过独立审查；Task 7 正按 RED→GREEN 实现；里程碑 1 尚未完成。

## 任务状态

| 任务 | 状态 | 提交 | 审查/备注 |
|---|---|---|---|
| Task 1：建立平台中立的显示器基础类型 | 已完成 | `e80ee84` | 首次审查：Needs fixes；fix round 1 已通过复审 |
| Task 2：固定 Per-Monitor V2 DPI 启动边界 | 已完成 | `65d1113` | 首次审查：Needs fixes；fix round 1 最终复审：Clean |
| Task 3：建立原生消息窗口与全局快捷键服务 | 已完成 | `ed04ed9` | 独立审查（`0490086`）：Clean |
| Task 4：实现可通知现有进程的单实例协议 | 已完成 | `7816279` | 独立审查（`13fd16a`）：Clean |
| Task 5：建立可刷新且可测试的显示器目录 | 已完成 | `203dd12` | 控制器台账确认 `13fd16a..4348900` 独立审查：Clean |
| Task 6：建立 D3D11 设备、WARP 降级与自有纹理 | 已完成 | `02da1ba` / `e140aae` | 控制器已确认独立审查：Clean |
| Task 7：实现 WGC 显示器单帧捕获服务 | 实施中 | — | 正在建立 RED 与生命周期测试 |
| Task 8：实现捕获业务状态与设备恢复 | 未开始 | — | — |
| Task 9：接入托盘、F2、显示器刷新与单实例启动 | 待独立审查 | 待提交 | 托盘/进程回归通过；desktop 验收仍由 Task 10 负责 |
| Task 10：建立真实桌面捕获验收并完成里程碑 | 未开始 | — | — |

## Task 1 实施记录

新增 `lc::platform::MonitorId`、`PhysicalPoint`、`PhysicalRect`、`PixelSize`，以及使用 64 位中间结果的 `width()`、`height()`、`isValid()`、`contains()`。物理矩形采用左闭右开语义；无效矩形不包含任何点。实现位于 `src/platform/MonitorTypes.hpp` 与 `src/platform/MonitorTypes.cpp`，测试位于 `tests/platform/MonitorTypesTests.cpp`，并已接入两个 CMakeLists。

### RED

命令（按主机约束通过包装器执行）：

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
```

结果：构建失败，测试编译明确报告 `platform/MonitorTypes.hpp` 不存在（C1083）；随后才进入 GREEN 实现。

### GREEN 与回归

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
```

结果：构建成功，生成 `landscapecutter_unit_tests.exe`。

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "physical rectangles" --output-on-failure
```

结果：3/3 通过。

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug --output-on-failure
```

结果：11/11 通过，总耗时 63.76 秒。

## Task 1 验证结论

- `git diff --check`：通过。
- 首次任务审查：Needs fixes，问题为本表提交字段误写“待提交”且缺少审查条目；fix round 1 已通过复审。
- 自审：文档修正未改动 Task 1 代码或测试；未新增依赖、未记录用户内容；接口、整数宽度和半开区间语义与简报一致。
- 桌面人工验收：不适用于本任务；真实桌面捕获留待后续任务。

## Task 2 实施记录

新增 `lc::platform::windows::ensurePerMonitorV2()` 与
`isPerMonitorV2()`，以及 `DpiSetupStatus` / `DpiSetupResult`。manifest 同时声明
Windows 10 compatibility、`PerMonitorV2` 与 `true/pm`，并由 RC 资源嵌入应用及独立
`landscapecutter_dpi_tests` 目标。为避免 MSVC 自动生成的 manifest 与 ID 1 资源冲突，两个
目标禁用链接器的自动 manifest，保留同一显式 RC manifest。`main()` 在收集参数并初始化
C++/WinRT STA apartment 后、构造 `QApplication` 前验证 DPI；失败时显示原生 `MessageBoxW`
并返回非零。

### RED

命令（按主机约束通过包装器执行）：

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_dpi_tests
```

结果：构建失败，`DpiAwarenessTests.cpp(1,10)` 明确报告
`platform/windows/DpiAwareness.hpp` 不存在（C1083），符合 DPI 类型/实现尚未添加的预期。

### GREEN、manifest 与回归

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug
```

结果：全量构建成功，生成 `LandscapeCutter.exe` 与 `landscapecutter_dpi_tests.exe`。

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "Per-Monitor V2|app_process_smoke" --output-on-failure
```

结果：2/2 通过，包括 `the process runs as Per-Monitor V2 before Qt exists` 和
`app_process_smoke`。

```powershell
& 'D:\Windows Kits\10\bin\10.0.28000.0\x64\mt.exe' -nologo `
  '-inputresource:out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe;#1' `
  "-out:$env:TEMP\LandscapeCutter.manifest.xml"
Select-String -Path $env:TEMP\LandscapeCutter.manifest.xml -Pattern "PerMonitorV2"
```

提取证据：`LandscapeCutter.manifest.xml:10` 包含
`<dpiAwareness ...>PerMonitorV2</dpiAwareness>`。

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug --output-on-failure
```

结果：12/12 通过；CTest 最终日志记录 `cmake_sdk_floor_behavior` 为第 12 项通过。

## Task 2 验证结论

- `git diff --check`：通过。
- 自审：只改动简报列出的启动、DPI、资源、测试、构建与进度文件；未改 Task 1 代码或测试，未新增第三方依赖。运行时只把 `ERROR_ACCESS_DENIED` 且当前上下文等于 Per-Monitor V2 视为可接受，其余失败会阻止 Qt 创建。

### 审查与未闭合验收

- 首次审查：Needs fixes。技术实现、manifest 提取、Per-Monitor V2 进程测试、应用烟雾测试和 CMake 配置均通过；唯一问题是任务状态未回填初始实现提交、缺少审查结论和未闭合验收记录。
- fix round 1：已修正上述进度文档；最终复审结论为 Clean。
- 未闭合验收（Task 10 或后续集成）：尚未执行真实桌面上的 Windows Graphics Capture 捕获；尚未在多显示器、混合 DPI 与负坐标布局中验证捕获输出和物理坐标映射；尚未完成捕获链路与后续托盘/恢复流程的端到端集成验收。这些项目不能由本 Task 2 的进程 DPI 边界测试替代，也不应记录为通过。

## Task 3 实施记录

新增 `lc::platform::windows::NativeMessageWindow`，它在 Qt 主线程创建不可见的顶层
`WS_POPUP` 窗口，扩展样式为 `WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`，固定类名为
`LandscapeCutter.NativeMessageWindow`。窗口不调用 `ShowWindow`，不是 `HWND_MESSAGE`，并把
`WM_HOTKEY`、`WM_DISPLAYCHANGE` 与 `WM_DEVICECHANGE` 转为类型化 Qt 信号；`WM_CLOSE` 请求 Qt
退出。HWND 和窗口类注册均由对象生命周期管理，析构断言在对象所属线程销毁 HWND，最后一个
窗口销毁后注销类。

新增 `GlobalHotkeyService`，接收调用方给出的 `HotkeyBinding` 并使用 `RegisterHotKey`。
重复注册先释放旧 binding；`ERROR_HOTKEY_ALREADY_REGISTERED` 映射为 `Conflict`，其他失败映射
为 `Failed`，析构会注销已注册的热键。服务只将当前 binding ID 的原生热键消息发为
`activated()`。本任务没有注册产品 `F2`、没有创建用户可见窗口，也没有接入捕获流程；Task 9
将以 `{0x4C43, MOD_NOREPEAT, VK_F2}` 作为产品 binding 完成 composition root 接入。

独立目标 `landscapecutter_platform_message_tests` 用自定义 Catch2 main 创建一个
`QCoreApplication`。它对真实 HWND 使用 `PostMessageW`，不创建任何用户可见 Qt 或 Win32 窗口。

### RED

首次仅加入原生消息窗口契约测试后，按主机约束通过包装器执行：

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_platform_message_tests
```

提升环境中的实际结果：编译在
`NativeMessageWindowTests.cpp(3,10)` 以 C1083 失败，明确缺少
`platform/windows/NativeMessageWindow.hpp`。受限环境首次运行被 MSBuild FileTracker
`E_ACCESSDENIED` 阻断，未将该环境错误误记为 RED。

原生消息窗口 GREEN 后加入全局热键契约测试，再次执行同一命令；编译在
`GlobalHotkeyServiceTests.cpp(1,10)` 以 C1083 失败，明确缺少
`platform/windows/GlobalHotkeyService.hpp`。两个 RED 均由尚不存在的生产接口导致。

### GREEN 与回归

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_platform_message_tests
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "native message|global hotkey" --output-on-failure
```

结果：目标构建成功，定向 CTest `4/4` 通过。原生窗口测试验证有效但不可见的顶层工具窗口、
热键 ID 转发及显示/设备变化广播；热键测试用 `VK_F24` 验证首次注册、重注册、第二窗口冲突、
精确一次激活和注销后的重新注册，不占用产品 `F2`。

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug --output-on-failure
```

结果：全量构建成功；CTest `16/16` 通过。

## Task 3 自审与未闭合验收

- 自审：仅改动 Task 3 列出的平台窗口/热键、测试、CMake 与进度文档；未改动 Task 1
  `MonitorTypes` 或 Task 2 DPI/manifest/main 启动边界，未新增第三方依赖。生产窗口没有可见样式
  或显示调用；测试仅使用 `VK_F24`。
- 实现提交：`ed04ed9`（`feat: add native messages and global hotkeys`）。独立审查记录提交
  `0490086`，结论为 Clean。
- 未闭合验收：产品 `{0x4C43, MOD_NOREPEAT, VK_F2}` 尚未在 Task 9 composition root 注册；
  尚未完成真实桌面捕获、多显示器/混合 DPI、托盘与退出流程的集成/人工验收。上述项目均不因
  本任务的自动测试而视为通过。

## Task 4 实施记录

新增 `lc::platform::windows::SingleInstanceCoordinator`，对固定产品 GUID
`4F4E6D0D-8C33-4B79-984A-3E44F6A62D11` 使用 `Local` 命名空间。`acquire()` 始终先创建自动
复位的 `.Activate` event，再创建并持有 `.Instance` mutex；mutex 已存在时返回 `Secondary`，否则
返回 `Primary`。测试后缀精确追加于 GUID 和两个对象后缀之间。次实例仅通过 `SetEvent` 通知；主
实例在 `QCoreApplication` 已存在时用 `QWinEventNotifier` 发出 `activationRequested()`。event、
mutex 都以 WIL `unique_handle` 管理，析构先关闭 notifier，再释放两个句柄。

独立目标 `landscapecutter_single_instance_tests` 以 GUID 建立每个测试的唯一命名空间，覆盖首个
协调器为 Primary、第二个为 Secondary；覆盖次实例在主实例建立 notifier 前 `SetEvent` 后仍恰好
触发一次；并在所有旧对象析构后验证同名 replacement 再次成为 Primary。

### RED

先加入测试目标和三项契约测试后，按主机约束通过包装器执行：

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_single_instance_tests
```

提升环境中的实际结果在 `SingleInstanceCoordinatorTests.cpp(1,10)` 以 C1083 失败，明确缺少
`platform/windows/SingleInstanceCoordinator.hpp`。受限环境的 FileTracker `E_ACCESSDENIED` 未记作
RED；同一包装器在允许的提升环境中取得了上述特征失败。

### GREEN 与回归

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_single_instance_tests
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "coordinator|secondary" --output-on-failure
```

结果：专用目标构建成功；定向 CTest `3/3` 通过。随后同一包装器的全量构建成功，完整 CTest
`19/19` 通过（含 `bootstrap_identity_behavior` 与 `cmake_sdk_floor_behavior`）。

## Task 4 自审与未闭合验收

- `git diff --check`：功能提交前通过；实现提交为 `7816279`
  （`feat: add single-instance activation protocol`）。
- 自审：event 在 mutex 前创建；命名完全使用任务书的 GUID 与后缀顺序；自动复位 event 的“先
  `SetEvent`、后监听”路径由真实内核对象和 Qt 事件循环验证；没有修改 `main()`、托盘、快捷键
  注册或捕获路径，未增加第三方依赖。
- 独立审查记录提交 `13fd16a`，结论为 Clean。未闭合验收：Task 9 才会将 acquire/secondary signal/
  primary listener 接入 `main()`，并以正式双进程验证次实例退出、现有实例提示以及进程/命名对象
  清理；真实桌面、多显示器/混合 DPI、托盘与捕获验收仍属后续任务。

## Task 5 实施记录

新增 `IDisplayTopologySource`、完整 topology record 与分层 `DisplayError`，并实现
`WindowsDisplayTopologySource`：组合 `EnumDisplayMonitors`、`GetMonitorInfoW`、
`GetDpiForMonitor(MDT_EFFECTIVE_DPI)`、`QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS)` 和
`DisplayConfigGetDeviceInfo`。所有原生返回码均检查，display config 缓冲区竞争以
`ERROR_INSUFFICIENT_BUFFER` 循环重试；DPI 必须大于零，HDR 取活动 target 的
`advancedColorEnabled`，没有把 capability 当作当前启用状态。

新增 `DisplayCatalog` 与规格第 5.1 节完整 `MonitorDescriptor`。refresh 以精确 GDI device name
连接 native monitor 与 active path，在局部完成重复/缺失映射验证、确定性 ID 构造和 descriptor
盖章，全部成功后才一次性发布并递增 generation。失败仅将目录标记为 unhealthy，旧快照与
generation 均保留。`monitorContaining()` 复用 Task 1 左闭右开物理矩形语义；
`monitorFromPoint()` 以 `MONITOR_DEFAULTTONEAREST` 取得瞬时句柄后复用目录查询。Monitor ID 由
固定宽度小写 LUID 十六进制、十进制 target ID 和 monitor device path 的 UTF-8 文本组成。

### RED

首次只加入内存 fake source 的六项规格测试与 CMake 接线，尚未创建生产接口。受限环境先被
MSBuild FileTracker `E_ACCESSDENIED` 阻断，该环境错误未计为 RED；随后在获准主机环境用指定
PowerShell 7 包装器重跑：

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
```

结果：`DisplayCatalogTests.cpp(1,10)` 以 C1083 明确报告缺少
`platform/windows/DisplayCatalog.hpp`。自审发现公开 `monitorFromPoint()` 需要直接测试后，先移除
其实现并加入契约测试；同一命令以 LNK2019 明确报告缺少
`DisplayCatalog::monitorFromPoint(POINT) const`，随后才恢复最小实现。

### GREEN、真实枚举与回归

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "display refresh|monitor lookup|monitor ids" --output-on-failure
```

结果：目标构建成功，简报原样定向正则 `4/4` 通过；加入两个 refresh 状态名称与 Win32 point
契约的扩展定向正则 `7/7` 通过。真实 Windows source 测试通过；安全诊断显示本环境有 `1` 台
活动显示器，物理矩形 `[0,0,1920,1080]`、DPI `96x96`、HDR `false`，未读取或输出画面、像素、
设备名或路径。

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug --output-on-failure
```

结果：全量构建成功；完整 CTest `27/27` 通过，总耗时 `86.53` 秒。

## Task 5 自审与未闭合验收

- `git diff --check`：通过；实现提交 `203dd12`（`feat: add physical display catalog`）。
- 自审：变更仅限简报列出的 platform、测试、两个 CMakeLists 与本进度文档；没有接入 `main()`、
  捕获或 UI，未新增第三方依赖。测试 expected 使用手写矩形、LUID、target ID 与 UTF-8 字节串；
  fake 仅提供完整数据，不断言 mock/fake 自身。
- 控制器台账已确认 Task 5（`13fd16a..4348900`）独立审查 Clean。未闭合验收：尚未在真实多显示器、负坐标、混合 DPI、
  HDR 开启/关闭和热插拔变化环境中人工验证目录刷新；`WM_DISPLAYCHANGE`/`WM_DEVICECHANGE` 尚未接入
  refresh；捕获、托盘和端到端真实桌面验收仍属后续任务。

## Task 6 实施记录

新增 `D3d11DeviceFactory`、`D3d11DeviceManager` 与 `TextureCopy`。factory 只请求 D3D feature
level 11.1/11.0，创建 flags 始终包含 BGRA support；Debug 构建缺少 Graphics Tools 时只对
`DXGI_ERROR_SDK_COMPONENT_MISSING` 去掉 debug flag 后重试。manager 每次 initialize/rebuild 都按
Hardware→WARP 尝试，在同一 mutex 临界区一次性发布 device bundle 与 generation；所有 immediate
context 操作也经过该 mutex 串行。复制操作拒绝跨设备纹理，将 owned 资源归一为 DEFAULT usage、
零 CPU access/零 misc flags；readback 使用 STAGING、零 bind、CPU read，并逐行按实际 row pitch
复制到受控 buffer。

### RED 与 TDD 限制

恢复开始时，设备降级、纹理复制及多数测试已经是未提交实现，前任没有留下可核验的原始 RED
日志，因此不把这些既有行为追记为本轮 test-first。接手后已有一项先于生产改动加入的快照一致性
测试；通过指定 PowerShell 7 包装器构建 graphics 目标，生产库先成功，测试随后在
`D3d11DeviceManagerTests.cpp` 以 C2039 明确报告 `D3dDeviceBundle` 缺少 `generation`。受限环境首次
运行的 MSBuild FileTracker `E_ACCESSDENIED` 仅为环境错误，不计为 RED。随后最小增加 bundle
generation，并在 manager 发布锁内同时赋值/替换/递增；同一目标重新构建成功。测试诊断只增加
安全元数据输出，不改变生产行为或断言。

### GREEN、真实设备与回归

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_graphics_tests
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "device manager|WARP|texture" --output-on-failure
```

结果：graphics 目标构建成功；简报定向正则 `8/8` 通过。真实 manager 详细运行输出
`driver=Hardware`、feature level `11.1`、generation `1`、BGRA flag `1`，证明本开发机优先选择
Hardware。真实 WARP `copyOwned` 在释放源纹理后完成 readback：BGRA8 为 `2x2`、format `87`、
rowPitch `8`；RGBA16F 为 `2x2`、format `10`、rowPitch `16`；两项均通过，未输出任何像素字节。

```powershell
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug
pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug --output-on-failure
```

结果：全量构建成功；完整 CTest 一次运行 `39/39` 通过，总耗时 `104.32` 秒。

## Task 6 自审与未闭合验收

- 实现提交：`02da1ba`（`feat: add shared D3D11 graphics device`）。
- 自审：变更仅限 Task 6 简报列出的 graphics、测试、两个 CMakeLists 与本进度文档；系统链接为
  `d3d11`、`dxgi`、`dxguid`、`windowsapp`，未新增第三方依赖。device+generation 同锁发布，
  immediate context 同锁串行；跨设备纹理、未初始化 manager、双 driver 失败、debug layer 缺失和
  owned/readback resource flags 均有检查或测试覆盖。错误结构只含 code 与 HRESULT；日志/测试输出
  不含截图像素或用户内容。
- 原始未提交实现缺少可核验的整体 RED，这是保留的 TDD 顾虑；本轮 generation 缺陷具有真实
  RED→GREEN 记录。控制器已确认 `e140aae` 独立审查为 Clean。
- 未闭合验收：WGC 单帧捕获、2000 ms 超时、设备丢失后每请求最多恢复一次、捕获状态协调、托盘/
  F2/单实例 composition root 及真实多显示器/混合 DPI/HDR 端到端验收均属于 Task 7–10，本任务
  没有实现或声明这些项目通过。

## Task 7 续作验证（2026-09-06）

WGC 实现已恢复。全量 Debug 构建成功；完整 CTest **59/59** 通过（88.83 秒），包含 20 项捕获服务、状态与 interop 测试。独立审查结论为 Clean。测试命令沿用上述 PowerShell7 构建包装器；MSBuild 沙箱访问拒绝经同一命令提升执行解决。真实桌面捕获及托盘集成尚未验收。

## Task 8 实施记录（2026-09-07）

新增 `CaptureCoordinator`，以当前鼠标显示器发起一次性捕获；捕获中请求只报告 Busy、不排队。它保存请求的显示器与 display/device generation，成功才替换最近帧，普通失败保留最近有效帧；首次 DeviceLost 清理旧设备帧、重建并只重试同一显示器一次，第二次丢失停止。结构化通知完整映射 `CaptureErrorCode`，availability 在可重入 notice 前发布；shutdown/析构取消活动请求、释放帧，并用 `QPointer` 抑制晚到回调。

### RED、GREEN 与回归

RED：先仅注册 `CaptureCoordinatorTests.cpp`，定向 unit build 在提升环境以 `CaptureCoordinator` 方法和 `staticMetaObject` 缺失的 LNK2019/LNK2001 失败；受限环境的 FileTracker `E_ACCESSDENIED` 未计为 RED。随后注册生产源，`landscapecutter_unit_tests` 构建成功。简报正则加 stale-device 覆盖的定向 CTest **7/7** 通过。完整 CTest 实际 **75/77** 通过：Task 8 的 15 项 coordinator 测试均通过；`app_process_behavior` 因 Task 9 草稿的 graceful-shutdown 行为失败，不能归入或在本任务范围内修复。完整 build 同样被 Task 9 `AppControllerTests.cpp` 所引用而尚未实现的 API 阻断；Task 8 unit target 已成功。

### 自审与未闭合验收

- 实现提交：`f9a4784`（`feat: coordinate one-shot monitor capture`）。
- `git diff --check`：通过。暂存前逐项确认仅含 Task 8 的 coordinator 源/测试、两处 CMake 注册和本进度记录；`tests/CMakeLists.txt` 仅暂存 `CaptureCoordinatorTests.cpp` 单行。
- 自审：重试使用原始 monitor descriptor，不重新按鼠标选择；所有 completion 先验证 shutdown、请求/attempt、catalog 与 device generation；availability 信号先于 notice；析构后的 retained completion 不解引用 coordinator。
- 未闭合验收：真实桌面、多显示器/混合 DPI/HDR 的 WGC 捕获，以及 Task 9 composition root/进程关闭行为仍待后续集成；它们不能由本任务 fake-service 单元测试替代。

### Fix round 1（2026-09-07）

新增连续成功完整替换、返回帧 display generation 过期、以及请求期间 current device generation 漂移三项独立测试。RED 定向 CTest 为 `2/3` 通过，`a current device generation drift releases the latest frame` 明确失败：旧 generation 3 最近帧仍存在。修复后 coordinator 只读取一次当前 device generation，在漂移时释放不属于当前 generation 的最近帧；确认 `DeviceLost` 时直接释放最近帧。GREEN 定向 CTest **7/7** 通过，完整 CTest 中 18 项 Task 8 coordinator 测试均通过；已知 Task 9 `app_process_behavior` graceful-shutdown 失败仍未在本任务范围内修改。

### Task 8: fix round 2/5（待复审，2026-09-07）

RED：新增 display 与 device generation 同时变化、completion 为 `DeviceLost` 的回归测试；定向 CTest `0/1`，`latestFrame()` 仍存在而失败。GREEN：在任何 display selector/catalog 早退前读取 current device generation，并先释放 generation 不匹配的最近帧；保持原有 DisplayChanged 通知优先级和不重建语义。unit build 成功，新回归 `1/1`，Task 8 定向正则 `22/22` 通过。自审：帧归属不变量现先于所有 display 检查建立；没有改动 Task 9/10 草稿。待复审并待真实桌面验收。

### Task 8: fix round 3/5（待复审，2026-09-07）

RED：分别增加 `DeviceLost` 同时遇 catalog generation 变化与 selector null 的测试，保持 recovery/latest frame generation 均为 3；定向 CTest `0/2`，两项均因旧帧残留失败。GREEN：身份校验后通过一次 `get_if<CaptureError>` 提前识别 DeviceLost 并清除最近帧，后续复用同一 error；保留 current-device invariant、DisplayChanged/DisplayUnavailable 通知优先级和不 rebuild 语义。unit build 成功，新回归 `2/2`，Task 8 定向正则 `24/24` 通过。自审：任何 display 早退都不能跳过 DeviceLost 的帧释放；未改动 Task 9/10 草稿。待复审并待真实桌面验收。

## Task 9 实施记录（2026-09-07，待独立审查）

已同步控制器结论：Task 8 在 `1d3bf7d` 独立复审 Clean。Task 9 将唯一 composition root 接入单实例、
托盘、原生消息窗口、物理鼠标坐标的显示器选择、D3D/WGC/capture coordinator 和 F2；次实例在 Qt、
托盘及图形对象之前仅 signal primary 并返回 0。托盘菜单在退出前提供稳定对象名
`captureCurrentMonitorAction` 的捕获项；WGC、目录或设备不可用时禁用并不注册 F2，F2 冲突时菜单
保留可用并通知。显示变化以 100ms single-shot timer 防抖刷新，退出 `aboutToQuit` 先关闭 coordinator
再注销热键。产品 binding 为 `{0x4C43, MOD_NOREPEAT, VK_F2}`；平台隔离测试仍使用 VK_F24。

### RED、GREEN 与回归

初始 RED 使用仓库 PowerShell 包装器完整构建：`AppControllerTests.cpp` 因 `setCaptureEnabled`、
`captureRequested`、`formatCaptureNotice` 与 `showHotkeyConflict` 尚未声明而 C2039/C2065 失败；这证明
失败来自缺失 Task 9 托盘契约。随后定向 CTest 的 `app_process_behavior` 在现有草稿下以
“Test product failed graceful shutdown.”失败，证明 WM_CLOSE 没有完成产品的优雅退出。受限环境的
MSBuild FileTracker `E_ACCESSDENIED` 只作为环境限制记录，未当作 RED。

F2 更新后，完整构建成功；定向 CTest `7/7` 通过，覆盖托盘顺序、可用性、action signal、成功通知、
F2 conflict、smoke 与正式双进程/helper 行为。helper 用每次唯一的 ready/stop event 与隐藏窗口占用 F2；
脚本启动前若发现真实产品进程即 return 125，且仅对它创建的 Process 发送 WM_CLOSE 或作受限兜底清理，
不会接触用户实例。非 desktop 完整回归和最终提交 SHA 将在本次独立审查前记录；真实桌面多屏/HDR
验收仍留给 Task 10。

- Task 9 实现提交：`68e7e864f32dff55c1dddb99eac0d5bef7763d24`（`feat: wire Windows capture into the tray app`）；独立审查待完成。

### Task 9: fix round 1/5（待复审，2026-09-07）

RED：先只增加纯 `CaptureRuntimePolicy` 的四项契约测试及 CMake 注册；构建以
`Cannot find source file: app/CaptureRuntimePolicy.hpp` 失败，明确指向尚不存在的生产策略。GREEN：
策略限制 refresh 只能在 Available/DisplayUnavailable 间恢复，Unsupported/DeviceUnavailable 跨两次
显示事件保持禁用；Registered/Conflict/Failed 显式三分流，Failed 进入不可自动恢复状态。`main()` 的
初始和防抖 refresh 路径均调用该策略；删除未使用 TextureCopy，并使 helper 的 Register/Unregister 同用
`{0x4C43, MOD_NOREPEAT, VK_F2}`。策略 + Task 9 focused CTest **11/11**，非 desktop CTest **90/90**，
`git diff --check` 均通过。Task 10 的 preset、desktop CMake hunk 与 integration 草稿未暂存；待独立复审。
