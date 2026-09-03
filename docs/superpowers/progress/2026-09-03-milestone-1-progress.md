# 里程碑 1 执行进度

- 实施计划：[里程碑 1：Windows 图形基础实施计划](../plans/2026-09-03-milestone-1-windows-graphics-foundation.md)
- 规格：[里程碑 1：Windows 图形基础详细设计](../specs/2026-09-03-milestone-1-windows-graphics-foundation-design.md)
- 执行分支：`codex/milestone-1-windows-graphics-foundation`
- 工作树：`D:\Projects\LandscapeCutter\.worktrees\milestone-1-windows-graphics-foundation`
- 当前任务：Task 2 fix round 1 正在修正进度文档；初始实现已提交，尚未复审；里程碑 1 尚未完成。

## 任务状态

| 任务 | 状态 | 提交 | 审查/备注 |
|---|---|---|---|
| Task 1：建立平台中立的显示器基础类型 | 已完成 | `e80ee84` | 首次审查：Needs fixes；fix round 1 已通过复审 |
| Task 2：固定 Per-Monitor V2 DPI 启动边界 | 已实现；fix round 1 文档修正中 | `65d1113` | 首次审查：Needs fixes（技术实现/manifest/DPI/CMake 均通过；进度状态缺口）；尚未复审 |
| Task 3：建立捕获帧值对象 | 未开始 | — | — |
| Task 4：实现 Windows 显示器目录适配器 | 未开始 | — | — |
| Task 5：实现 Windows Graphics Capture 捕获适配器 | 未开始 | — | — |
| Task 6：实现捕获协调器 | 未开始 | — | — |
| Task 7：接入托盘捕获命令 | 未开始 | — | — |
| Task 8：处理显示器热插拔与捕获失败 | 未开始 | — | — |
| Task 9：完成多显示器与 DPI 验收 | 未开始 | — | — |
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
- fix round 1：正在修正上述进度文档；尚未复审，不预写“通过”。
- 未闭合验收（Task 10 或后续集成）：尚未执行真实桌面上的 Windows Graphics Capture 捕获；尚未在多显示器、混合 DPI 与负坐标布局中验证捕获输出和物理坐标映射；尚未完成捕获链路与后续托盘/恢复流程的端到端集成验收。这些项目不能由本 Task 2 的进程 DPI 边界测试替代，也不应记录为通过。
