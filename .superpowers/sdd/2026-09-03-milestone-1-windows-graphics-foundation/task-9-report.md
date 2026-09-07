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
