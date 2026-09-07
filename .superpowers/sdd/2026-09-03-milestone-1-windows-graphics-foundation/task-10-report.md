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

结论：**DONE_WITH_CONCERNS**。自动与真实桌面验收已通过，但必须**等待用户人工验收** F2/托盘通知、
快速重复 F2、helper 冲突降级、第二实例以及退出后无残留矩阵；在用户确认前，不得宣称里程碑完成。
