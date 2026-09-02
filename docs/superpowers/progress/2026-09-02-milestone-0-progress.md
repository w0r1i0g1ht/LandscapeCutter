# 里程碑 0 执行进度

## 基本信息

- 实施计划：[里程碑 0：C++ 工程基础实施计划](../plans/2026-09-02-milestone-0-cpp-foundation.md)
- 设计文档：[LandscapeCutter C++ 重构设计](../specs/2026-09-02-cpp-rewrite-design.md)
- 执行分支：`codex/cpp-foundation`
- 隔离工作树：`D:\Projects\LandscapeCutter\.worktrees\cpp-foundation`
- 执行方式：每个任务由独立子代理实施，随后进行规格符合性与代码质量审查
- 当前状态：Task 6 执行准备中
- 完成度：5 / 6

## 任务状态

| 任务 | 状态 | 实施提交 | 审查结果 |
|---|---|---|---|
| 任务 1：保留 Python 原型 | 已完成 | `bdb76c8` | 规格符合、质量通过 |
| 任务 2：建立可复现的构建工具链 | 已完成 | `c04fcb4`、`222ddd5`、`6c8555d`、`8ef5741` | 规格符合、质量通过（2 轮修复） |
| 任务 3：以测试驱动方式建立应用接口约定 | 已完成 | `e1c79e7`、`c3a5494` | 规格符合、质量通过（1 轮修复） |
| 任务 4：构建 Qt 托盘应用空壳 | 已完成（交互验收待具备托盘的桌面） | `122fe99` | 规格符合、质量通过 |
| 任务 5：移除当前 Python 运行实现并重写项目文档 | 已完成 | `fb0fc71` | 规格符合、质量通过 |
| 任务 6：从干净构建目录验证里程碑 0 | 未开始 | — | — |

## 执行记录

- 2026-09-02：用户批准设计文档和中文实施计划，并选择子代理驱动执行。
- 2026-09-02：创建隔离分支 `codex/cpp-foundation` 和项目内 worktree。
- 2026-09-02：完成计划预检；任务接口、文件交接和测试顺序未发现冲突。
- 2026-09-02：已派发任务 1，开始创建 Python 原型恢复标签和迁移说明。
- 2026-09-02：任务 1 已完成并通过独立审查；`python-prototype-final` 已确认为带注释标签，目标为 `74a1184`，并包含完整 Python 原型文件。
- 2026-09-02：任务 2 已进入执行阶段，开始建立固定版本的 CMake、vcpkg 与 Qt 构建工具链。
- 2026-09-02：任务 2 已提交构建基础 `c04fcb4` 并进入独立审查；固定标签 vcpkg 已完成克隆和引导，但当前主机缺少 Visual Studio 2022/17.x，依赖安装与 CMake 配置尚未通过。
- 2026-09-02：任务 2 独立审查要求修复两个问题：无参数引导失败（Critical）和未约束 Windows SDK 10.0.19041 下限（Important）；已进入第 1 轮修复。
- 2026-09-02：任务 2 第 1 轮修复提交 `222ddd5` 已通过范围复审；两个发现均已解决且未引入新问题。任务仍等待 VS 2022/17.x，以完成依赖安装、配置和构建验收。
- 2026-09-02：用户批准复用现有 Visual Studio 2026，不再安装 VS 2022 Build Tools；工具链修订为 CMake 4.4+、vcpkg `2026.07.29` 和 Qt 6.8.2 override。
- 2026-09-02：用户确认书面设计修订；中文实施计划已改为 VS 2026，并重新打开 Task 2 中受工具链变化影响的步骤。
- 2026-09-02：已恢复 Task 2 原实施子代理，开始迁移 vcpkg、安装 Qt 6.8.2#2 并验证 VS 2026 空工程构建。
- 2026-09-02：VS 2026 工具链修订提交 `6c8555d`；依赖安装、CMake 配置和空工程构建已成功，现进入独立审查。
- 2026-09-02：Task 2 独立审查发现 clean bootstrap 的浅克隆无法解析历史 Qt port tree（Critical）；已裁定改为完整固定标签克隆并进入第 2 轮修复。
- 2026-09-02：Task 2 第 2 轮修复提交 `8ef5741` 已通过范围复审；clean bootstrap、依赖解析、配置和构建全部通过，Task 2 完成。
- 2026-09-02：已派发 Task 3 独立实施子代理，开始按 RED→GREEN→REFACTOR 建立元数据和启动参数接口。
- 2026-09-02：Task 3 提交 `e1c79e7`；RED 失败原因正确，全量测试 4/4 通过，现进入独立审查。
- 2026-09-02：Task 3 审查确认实现符合规格，但计划定向正则漏测两个用例（Important、plan-mandated）；已进入第 1 轮修复。
- 2026-09-02：Task 3 第 1 轮修复提交 `c3a5494` 已通过范围复审；定向测试与全量测试均为 4/4，Task 3 完成。
- 2026-09-02：Task 4 提交 `122fe99`，完成 Qt 托盘控制器、GUI 入口、资源和进程冒烟测试；首次运行暴露构建目录缺少 Qt Windows 平台插件，已在目标构建后部署 `qwindowsd.dll`。
- 2026-09-02：Task 4 控制器重新配置、构建并运行 CTest，5/5 通过；独立审查确认规格符合且代码质量通过，无 Critical、Important 或 Minor 问题。
- 2026-09-02：普通模式在当前桌面正确进入“系统托盘不可用”保护；Win32 只读探测确认该桌面不存在 `Shell_TrayWnd`、`Shell_SecondaryTrayWnd` 和 `NotifyIconOverflowWindow`，真实托盘图标、通知和“退出”菜单验收顺延到 Task 6 或具备通知区域的桌面。
- 2026-09-02：Task 5 提交 `fb0fc71`；再次确认带注释标签可恢复完整 Python 原型后，按计划精确删除 15 个 Python 运行与旧设计路径，并更新公开 README 和历史伪最小化说明。
- 2026-09-02：Task 5 控制器独立验证当前分支无已跟踪 Python 运行路径、C++ 构建成功、CTest 5/5；独立审查确认 15 个删除和 2 个文档修改均精确符合规格，无 Critical、Important 或 Minor 问题。

## 当前阻塞

- Task 4/Task 6 的真实托盘交互验收需要具备 Windows 通知区域的桌面；当前执行桌面没有系统托盘窗口。该环境限制不阻塞 Task 5 的仓库迁移工作。

## 验证汇总

- 任务 1：`python-prototype-final` 类型为 annotated tag，目标提交为 `74a1184dc41069cd5791b37a18f00242c91ed78d`。
- 任务 1：标签内共核验 13 个原型路径，包含 Python 源码、`requirements.txt`、`config.json`、已跟踪 `.pyc` 和 `.pyd`。
- 任务 1：提交 `bdb76c8` 只新增 `legacy/README.md`；独立审查无 Critical、Important 或 Minor 问题。
- 任务 2 初始方案：vcpkg 仓库曾固定到 `2025.02.14`，`vcpkg.exe` 已生成，`.tools/` 与 `out/` 已确认被 Git 忽略；该方案随后被 VS 2026 修订取代。
- 任务 2 初始方案：依赖安装曾失败于“Unable to find a valid Visual Studio instance”；CMake 配置曾失败于“Visual Studio 17 2022 could not find any instance”，随后按用户批准改用 VS 2026。
- 任务 2：修复后无参数 bootstrap 已进入实际 vcpkg 检查；Windows SDK 下限 `10.0.19041.0` 已在配置期约束，并允许更高版本。
- 任务 2：第 1 轮范围复审结论为“全部发现已解决，无新的 Critical/Important 问题”。
- 工具链修订探测：CMake 4.4.3 已提供 `Visual Studio 18 2026` 生成器；vcpkg `2026.07.29` 的构建脚本识别 `VisualStudioVersion` 18.x。
- 工具链修订探测：vcpkg `2026.07.29` 的版本数据库保留 `qtbase 6.8.2`，最高 port-version 为 2，可在新 baseline 上显式 override。
- Task 2 实际验证：CMake 使用 VS18、MSVC 19.51.36256、Windows SDK 10.0.28000.0，并成功解析 `qtbase 6.8.2#2`。
- Task 2 实际验证：`cmake --preset windows-msvc-debug` 和授权后的 `cmake --build --preset windows-msvc-debug` 均成功。
- Task 2 中间审查事项（已解决）：`--depth 1` 克隆无法直接解析 Qt 6.8.2#2 历史 port tree，本次通过补全 vcpkg 历史后成功。
- Task 2 最终验证：从空 `.tools/vcpkg` 运行单条无参数 bootstrap，无需手工补历史即可解析 `qtbase 6.8.2#2`；vcpkg 仓库不是浅克隆。
- Task 2 最终审查：浅克隆 Critical 已解决，无新的 Critical、Important、Minor 或范围外问题。
- Task 3：RED 正确失败于缺少生产头文件；GREEN 构建无警告，修正后的定向测试和全量 CTest 均为 4/4。
- Task 3：范围复审确认计划验证缺陷已解决，无新的 Critical、Important 或范围外问题。
- Task 4：首次进程启动失败于 Qt Windows 平台插件未部署；根因确认后，构建目标会将配置对应的 `Qt6::QWindowsIntegrationPlugin` 复制到可执行目录的 `platforms` 子目录。
- Task 4：修复后控制器独立验证 CMake 配置成功、沙箱外 MSBuild 成功、CTest 5/5 通过，`app_process_smoke` 在 0.07 秒内退出。
- Task 4：独立审查结论为规格符合、代码质量通过，无 Critical、Important 或 Minor 问题；真实托盘交互仅因当前桌面无通知区域而未验证。
- Task 5：`python-prototype-final` 仍为 annotated tag，目标为 `74a1184dc41069cd5791b37a18f00242c91ed78d`，并包含全部待删除 Python 运行路径。
- Task 5：当前分支 `git ls-files -- python requirements.txt config.json` 无输出；构建成功，CTest 5/5 通过。
- Task 5：独立审查确认变更精确包含 15 个计划删除路径和 2 个文档修改路径，无额外删除或遗漏。
