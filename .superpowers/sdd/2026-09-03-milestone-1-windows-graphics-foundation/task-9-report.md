# Task 9 报告：托盘、F2、刷新与单实例

## 改动

- `main()` 作为唯一 composition root：在 Qt 前完成 WinRT、Per-Monitor V2 与单实例判定；次实例只
  signal primary 后返回。主实例按托盘、原生消息窗口、显示目录、D3D、WGC/coordinator、F2 热键顺序装配。
- 新增托盘捕获 action（`captureCurrentMonitorAction`）、结构化通知、单实例/F2 冲突/兼容模式通知；托盘和
  F2 均连接到同一个 `CaptureCoordinator::requestCapture()`。
- 每次捕获通过 `GetCursorPos` 和 `DisplayCatalog::monitorFromPoint()` 选择物理显示器；显示变化经 100ms
  single-shot 防抖 refresh。能力失效时禁用 action 并注销热键；F2 conflict 保持菜单可用。
- 新增 WGC 静态 `MonitorCaptureService::isSupported() noexcept` 查询（账本裁定允许），进程 helper 和安全
  的双进程/F2 conflict 脚本。平台层既有 VK_F24 隔离测试未修改。

## RED

使用指定包装器：

```powershell
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug
```

结果：`AppControllerTests.cpp` 因 `AppController` 缺少 `setCaptureEnabled`、`captureRequested`、
`formatCaptureNotice`、`showHotkeyConflict` 而 C2039/C2065 失败，属于缺失 Task 9 托盘行为。

```powershell
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "tray menu|capture availability|app_process_behavior" --output-on-failure
```

结果：`app_process_behavior` 失败，`Test product failed graceful shutdown.`，证明 WM_CLOSE 未完成优雅退出。
首次受限构建的 FileTracker `E_ACCESSDENIED` 是环境限制，未计为 RED；提升后取得上述契约失败。

## GREEN 与验证

```powershell
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "tray menu|capture availability|capture action|structured success|F2 hotkey|app_process_behavior|app_process_smoke" --output-on-failure
```

结果：构建成功；focused CTest **7/7 passed**，其中正式 `app_process_behavior` 通过。

```powershell
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -LE desktop-integration -j 8 --output-on-failure
git diff --check
```

结果：非 desktop CTest **86/86 passed**；`git diff --check` 通过。

## 进程隔离、暂存范围与自审

进程脚本首先检查真实 `LandscapeCutter`；若存在即以 CTest skip code 125 返回，绝不关闭它。它只对脚本
创建的 `Process` 发送 `WM_CLOSE`，helper 使用每次唯一命名 ready/stop event 正常退出；只在该测试创建
进程无法优雅退出时才作兜底 kill。暂存仅含 Task 9 的 app/main、最小 isSupported、helper/script、托盘测试、
tests CMake 的 helper/process hunk、进度与本报告；明确排除 `CMakePresets.json`、`tests/integration/` 与
`tests/CMakeLists.txt` 的 desktop-integration hunk。

自审：未实现保存、剪贴板、预览、选区或像素输出；退出顺序为 coordinator shutdown 后 hotkey unregister；
F2 冲突不会禁用菜单；成功文案包含显示器名、物理尺寸与格式。

## 已知顾虑

Task 10 仍负责 desktop-integration、真实多屏/混合 DPI/HDR 与人工托盘通知验收。本 Task 9 记录待独立审查。

Task 9 实现提交：`68e7e864f32dff55c1dddb99eac0d5bef7763d24`

## Fix round 1（待独立复审）

### RED

先增加 `CaptureRuntimePolicyTests.cpp` 与 Task 9 CMake 注册，未创建 production policy 即执行：

```powershell
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
```

结果：CMake 以 `Cannot find source file: app/CaptureRuntimePolicy.hpp` 失败，明确是缺失策略接口。
中断后的首次 GREEN 构建保留了该不完整生成状态并报旧库 LNK2019；重新配置后才作为环境/生成恢复处理，
不计为新的行为 RED。

### GREEN、改动与自审

新增纯 `CaptureRuntimePolicy`，由 `main()` 的初始能力判定与 100ms refresh callback 共同调用：仅
`Available`/`DisplayUnavailable` 可以因显示器 refresh 改变；`Unsupported` 与
`DeviceUnavailable` 在任意显示事件后保持禁用。refresh 的 hotkey 结果按 Registered/Conflict/Failed
三分流，只有 Conflict 提示 F2 且保留菜单；Failed 进入不可自动恢复的 `DeviceUnavailable`。移除了
未使用的 `TextureCopy` 局部对象和 include。helper Register/Unregister 统一使用
`{0x4C43, MOD_NOREPEAT, VK_F2}`。

```powershell
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -R "display refresh becomes|device unavailable remains|unsupported capture remains|refresh hotkey outcomes|tray menu|capture availability|capture action|structured success|F2 hotkey|app_process_behavior|app_process_smoke" --output-on-failure
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug -LE desktop-integration -j 8 --quiet
git diff --check
```

结果：策略 + 原 7 项 focused **11/11 passed**；非 desktop CTest **90/90 passed**；`git diff --check`
通过。自审确认策略不是测试复制逻辑，main 的 initial/refresh 均直接调用；未暂存 Task 10 preset、
desktop CMake hunk 或 integration 草稿。冲突通知单元文案覆盖为已记录 deferred minor，未扩大本轮范围。
