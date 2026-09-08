# 里程碑 1 最终审查修复报告（2026-09-08）

## 基线与范围

- Fix base：`0edec45cc40226095e278fa38cb0cdb3ec932c99`。
- 仅处理最终整分支审查的 3 个 Important：portable preset、runtime 前置条件丢失、不可用通知缺失。
- 未处理已 deferred 的 6 个 Minor；未改捕获像素、WGC/D3D 实现、README 或其他任务文件。
- 工作树开始时 clean；本波次只修改获准的 preset、runtime policy、composition root、相关测试、
  CMake test 注册与进度/报告文件。

## Finding 1：portable preset

### RED

先新增 `tests/scripts/CMakePresetPolicyTests.ps1` 并在 `tests/CMakeLists.txt` 注册。测试把真实
`CMakePresets.json` 复制到临时 relocated checkout，在新 `${sourceDir}` 下创建 sentinel toolchain，
再实际执行 `cmake --preset windows-msvc-debug`。首次脚本运行因 Windows PowerShell 的旧
`ProcessStartInfo` 不支持 `ArgumentList` 而报错，此为测试 harness 错误，不计作 RED；改用兼容的
`Arguments` 后重新运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/scripts/CMakePresetPolicyTests.ps1 `
  -CMakeCommand 'C:\Program Files\CMake\bin\cmake.exe' -SourceRoot (Get-Location).Path
```

结果：exit 1，临时 checkout configure 到达测试的 `CMakeLists.txt`，明确报
`The configured toolchain did not resolve from the relocated source directory.`；证明当前绝对路径绕过了
新 checkout 的 sentinel toolchain。

### GREEN

将 preset 精确改为 `${sourceDir}/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake`。同一命令 exit 0，输出
`The Windows preset resolved its toolchain from the relocated source directory.`。默认全套中的
`cmake_preset_portability_behavior` 也通过。

## Finding 2：保留独立 capture 前置条件

### RED

先只增加 `initial(true,false,false)` 的 display refresh 序列：

```powershell
cmake --build --preset windows-msvc-debug --target landscapecutter_unit_tests
ctest --preset windows-msvc-debug -R 'combined catalog and device failure' --output-on-failure
```

结果：build 通过，CTest **0/1**；成功 refresh 后 availability 实际为 Available 而不是
DeviceUnavailable，且 enabled/registerHotkey 都错误为 true，共 3 个预期断言失败。随后先增加独立
前置条件与 coordinator 同步的 wished-for API 测试，compile RED 明确缺少 `captureSupported`、
`catalogReady`、`deviceReady` 与 `afterAvailabilityChanged`。

### GREEN

`CaptureRuntimeState` 独立保存 support/catalog/device；所有派生 availability、menu enable 与 F2
注册均从三项前置条件重新计算。display refresh 只更新 catalog；hotkey Failed 与 coordinator
DeviceUnavailable 将 device 前置条件写为 false。定向策略回归 **9/9** 通过，覆盖关键组合、连续失败/
成功 refresh、Unsupported、coordinator DeviceUnavailable、hotkey Failed 与 Conflict 三分流。

## Finding 3：结构化不可用通知与真实装配

### RED

先增加以下行为测试后构建 policy/controller targets：

```powershell
cmake --build --preset windows-msvc-debug --target `
  landscapecutter_unit_tests landscapecutter_app_controller_tests
```

结果：compile RED，明确缺少 `CaptureRuntimeState::pendingNotice` 和
`CaptureRuntimePolicy::takePendingNotice`。测试覆盖三种启动不可用 code、运行中 display failure 的
状态变化/重复失败语义、新显露 device failure、hotkey Failed、coordinator 不重复通知，以及三种 code
现有格式化文本互相区分。

### GREEN

policy 产生 `std::optional<CaptureNoticeCode>`，只在启动不可用或非 Available 状态发生变化时设置；
`takePendingNotice` 使用 move/exchange 保证一次性消费。coordinator availability 同步只更新前置条件，
不替 coordinator 自己重复生成 notice。`main()` apply 先复制/消费 pending notice，再设置菜单和
coordinator，最后实际调用 `AppController::showCaptureNotice(CaptureNotice{.code = ...})`；同步
`availabilityChanged` 因此无法在展示前清掉已复制值。通知/格式 focused CTest **5/5** 通过。

## Fresh 验证

所有 CMake/MSBuild/CTest 命令通过同目录 `Invoke-CleanBuildTool.ps1` 在 FileTracker 可用环境执行。
linked worktree 的 canonical path 只用于如下命令行 override，没有提交或写入全局配置：

```powershell
cmake --fresh --preset windows-msvc-debug --toolchain D:/Projects/LandscapeCutter/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug -R 'failed display refresh becomes|device unavailable remains|unsupported capture remains|refresh hotkey outcomes|combined catalog and device|initial policy preserves|coordinator device unavailability|failed hotkey registration|startup unavailability|display refresh failure notifies|newly revealed device|coordinator availability synchronization|controller honors|tray menu exposes|capture availability enables|triggering the tray|structured success|startup capture prerequisites|an F2 hotkey conflict|app_process' --output-on-failure
ctest --preset windows-msvc-debug --output-on-failure
ctest --preset windows-msvc-debug-desktop --output-on-failure
```

- fresh configure：通过；MSVC `19.51.36256.0`，Windows SDK `10.0.28000.0`，Qt `6.8.2`。
- 完整 build：通过。
- Task 9 runtime/controller/process focused：**21/21**，含 `app_process_smoke` 与
  `app_process_behavior`。
- 默认非 desktop：**100/100**，含 bootstrap、SDK floor、preset portability。
- desktop：**2/2**，0.86 秒。
- process suite 前后：`LandscapeCutter=0`、`landscapecutter_hotkey_occupier=0`；未留下产品/helper。
- `git diff --check`：通过（exit 0；仅有 Git 的 LF→CRLF 工作树提示，无 whitespace error）。

## 自审与顾虑

- preset test 运行真实 relocated configure，而非 grep 字符串；临时目录校验位于系统 temp 下再删除。
- support/catalog/device 的优先级保持既有 `Unsupported > DisplayUnavailable > DeviceUnavailable > Available`；
  同时 catalog/device 失败时，catalog 恢复会显露并通知仍存在的 DeviceUnavailable，但不会启用菜单/F2。
- pending notice 在 coordinator setter 前消费，显式覆盖同步 reentrant signal；coordinator capture completion
  仍按原路径发自己的 `noticeReady`，没有重复来源。
- F2 Conflict 保留专用 `showHotkeyConflict()`；Failed 进入持久 DeviceUnavailable 并显示对应反馈。
- 3 个 Important 已修复并完成本波次自审；结论仍为**待 scoped re-review**，未提前宣称最终整分支 Clean。
- 保留既有环境顾虑：本机仅覆盖单显示器 SDR；多显示器、混合 DPI、负坐标与 HDR 仍未覆盖，属于原 Task
  10 已披露限制，不是本波次新增回归。
