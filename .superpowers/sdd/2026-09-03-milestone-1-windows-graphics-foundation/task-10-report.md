# Task 10 自动与真实桌面证据（F2，2026-09-07）

## 变更与自审

- `DesktopCaptureIntegrationTests.cpp` 使用完整非空 `DisplayCatalog`，每个活动显示器发起
  `2000ms` 请求和 `2500ms` 外层 `QEventLoop` 限时；超时回调先 `cancel()`。completion 只将
  `CaptureFrame` 按值移动到 fixture，错误不作为帧继续处理；服务完成后再对 owned texture readback。
- 测试只记录显示器数量、物理尺寸、DPI、HDR 和耗时；不保存、不打印像素或 `TextureReadback::bytes`。
  断言覆盖 source/display/device generation、SDR/HDR 格式、DEFAULT owned texture 和关闭 session
  后的 readback 尺寸、row pitch、buffer 总长度与格式。
- `windows-msvc-debug` 排除 `desktop-integration`；`windows-msvc-debug-desktop` 只包含该 label。
  preset 的 toolchain 固定为 canonical
  `D:/Projects/LandscapeCutter/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake`。
- README 说明 F2 仅产生内存单帧和尺寸通知，文件保存/剪贴板属于里程碑 2；不把未观察的人工行为
  写作已通过。进度已同步 Task 9 review Clean、本轮自动证据、环境缺口与人工矩阵。

## Fresh 验证

先用 PowerShell `Resolve-Path` 与 `GetFullPath` 验证目标精确为工作树内
`out/build/windows-msvc-debug`，再删除该子目录；没有删除任何宽泛路径。CMake/MSBuild/CTest 均经：

```powershell
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 <cmake|ctest> ...
```

执行命令及摘要：

```powershell
# clean configure（canonical toolchain 由 preset 提供）
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --preset windows-msvc-debug
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug --target landscapecutter_desktop_capture_tests
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug-desktop --output-on-failure
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 cmake --build --preset windows-msvc-debug
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug --output-on-failure
C:\Users\13195\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe -NoProfile -File .superpowers/sdd/2026-09-03-milestone-1-windows-graphics-foundation/Invoke-CleanBuildTool.ps1 ctest --preset windows-msvc-debug-desktop --output-on-failure
```

- configure 与 full build：成功（MSVC 19.51.36256.0、Windows SDK 10.0.28000.0、Qt 6.8.2）。
- 默认 CTest：**90/90** 通过；desktop CTest：**2/2** 通过，0.58 秒。
- 真实 desktop metadata：活动显示器 `1`；物理 `3200x2000`；DPI `192x192`；HDR `false`
  （SDR/BGRA8）；两项逐屏完成耗时 `131ms` 与 `118ms`，均满足 `150ms` 目标。
- 多显示器、混合 DPI、负坐标布局、HDR：**环境不具备**，未伪装为通过。
- `git diff --check`：通过。vcpkg verify-only 从 `scripts/bootstrap.ps1` 的锁定值复核：tag
  `2026.07.29`、tag object `c76c06644034521fb761a39f8f52d8e87d1103d5`、peeled commit/HEAD
  `9e593bb18ea69cc5095e012465dcd675a822ed0d`、tracked status clean、特殊 index 位数 `0`。

## 暂存范围与结论

暂存且提交仅限 Task 10 所有权文件：desktop integration fixture、其 `tests/CMakeLists.txt` hunk、
`CMakePresets.json`、`README.md`、里程碑进度和本报告；不含生产模块、`out/`、`.tools/` 或其他任务文件。

结论：自动与真实桌面验收已通过，随后记录人工验收与控制器退出核验。

## 人工验收与控制器退出核验（2026-09-07）

用户在当前单屏确认：F2 的通知与捕获正常，显示器名称、`3200x2000` 和 `SDR BGRA8` 正确；托盘
“捕获当前显示器单帧”正常；快速重复 F2 时应用保持响应、未崩溃或卡死。第二实例自动以 `0` 退出，
用户第二次观察确认既有实例收到“LandscapeCutter 已在运行”通知。helper 占用 F2 后，用户确认
“F2 已被其他程序占用”通知出现，托盘捕获项仍可用并成功捕获。用户从托盘退出后，图标立即消失，
无灰色残影。

控制器退出核验为：`ProductProcessCount=0`、`HelperProcessCount=0`、`NativeWindowCount=0`、
`ProductNamedObjectsAbsent=True`。自动证据不改写：提交 `a333e4f` 的 fresh build、默认 CTest
**90/90**、desktop CTest **2/2**；单屏 `3200x2000`、`192x192 DPI`、SDR，耗时 `131ms/118ms`。
多显示器、混合 DPI、负坐标和 HDR 仍为**环境未覆盖**。

结论：**DONE**。Task 10 与里程碑 1 验收完成；F2 仍只捕获内存单帧并通知尺寸，文件保存与剪贴板
仍属于里程碑 2。

## Fix round 1/5：文档状态一致性（2026-09-07）

Important finding：进度顶部已称 Tasks 1–10 与里程碑 1 完成，但状态表仍将 Task 7 写为“实施中”、
Task 8 写为“未开始”，并把已提交的 Task 10 docs 提交标作“待提交”，造成自相矛盾。

修复：Task 7 更新为已完成，提交 `7416b6e`、独立审查 Clean；Task 8 更新为已完成，提交
`f9a4784` / `1d3bf7d`、独立复审 Clean；Task 10 提交列更新为 `a333e4f` / `0e82f29`，移除“待提交”。
Task 10 审查栏仅如实记录独立复审正在修复本文档一致性，未提前标记最终 Clean。

```powershell
git diff --check
```

结果：通过。自审：只修改进度状态表与本报告；没有改动代码、测试、README、CMake 或 preset，自动
测试数维持默认 **90/90** 与 desktop **2/2**。desktop 循环缺少 `CAPTURE` 上下文为主 Agent 已 deferred
的 Minor，本轮未修改测试。

## Fix round 1 独立复审结果

- Important：ADDRESSED。
- New Breakage：None。
- 最终 verdict：All findings addressed。

## 最终审查后的 preset 证据澄清（2026-09-08）

本报告前面的 Task 10 历史证据使用了**当时错误提交的绝对 toolchain preset**；这些命令与结果按
当时事实保留，不回写成后来才有的 portable 配置。最终整分支审查指出该 preset 破坏其他 checkout
后，`CMakePresets.json` 已恢复为精确的
`${sourceDir}/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake`，并新增 relocated-checkout 行为回归。

最终修复波次重新验证时，fresh configure 从 portable preset 出发；由于本 linked worktree 的 Qt/vcpkg
本机环境需要 canonical path，只在本次命令行使用：

```powershell
cmake --fresh --preset windows-msvc-debug --toolchain D:/Projects/LandscapeCutter/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake
```

该 override 未写回 preset 或全局配置。重新验证结果为完整 build 通过、默认非 desktop **100/100**、
desktop **2/2**；详细证据位于同目录 `final-fix-report.md`。这项澄清不改变 Task 10 在 2026-09-07
记录的原始命令、单屏 metadata 或人工验收事实。
