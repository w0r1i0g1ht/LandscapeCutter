# LandscapeCutter

一款面向 Windows 10/11 的截图、贴图与窗口相对区域实时监视工具。

LandscapeCutter 正在从 Python 原型重构为 C++20 与 Qt 6。项目将先完成可靠的静态截图、
标注和贴图工作流，再以“绑定目标窗口相对区域的实时贴图”作为特色能力。

## 当前状态

**里程碑 0、里程碑 1 和里程碑 2 已完成。** 当前仓库已经切换到 C++/Qt 工程，
具备可复现的构建环境、自动测试和可运行的 Windows 系统托盘程序。

当前版本按 F2 会先冻结全部显示器，再显示矩形选区。选区支持创建、移动和八方向缩放；进入标注
后可使用矩形、椭圆、箭头、画笔、文字和马赛克，并可撤销、重做、复制或保存为 PNG/JPEG。自动测试
覆盖离屏多显示器拼接、标注导出和会话清理；真实多显示器、混合 DPI 与 HDR 硬件仍需补充人工验收。

已经完成并验证的基础能力：

- C++20、Qt 6.8.2、CMake Presets 与固定版本 vcpkg 工具链；
- Windows x64 系统托盘程序、启动通知和右键“退出”；
- 系统托盘暂时不可用时的延迟注册；
- Qt Windows 平台插件和 ICO 图像插件随构建产物部署；
- Python 原型通过 Git 标签保留，主分支不再包含 Python 运行实现；
- 默认自动测试与独立真实桌面捕获测试均通过；F2、托盘通知、冲突降级、单实例与退出清理
  已完成人工验收。

## 路线图

- [x] **里程碑 0：C++ 工程基础**——冻结 Python 原型，建立可构建、可测试的托盘程序。
- [x] **里程碑 1：Windows 图形基础**——D3D11、Windows Graphics Capture、显示器模型、
  Per-Monitor V2 DPI、全局快捷键和单实例；自动、真实桌面与人工验收均已完成。
- [x] **里程碑 2：静态截图闭环**——冻结多显示器快照、矩形选区、复制和图片保存；自动、真实桌面与当前环境人工验收均已完成。
- [ ] **里程碑 3：标注系统**——六类工具与统一输出已完成自动验证，等待人工验收。
- [ ] **里程碑 4：静态贴图**——多个无边框置顶贴图及拖动、缩放和透明度控制。
- [ ] **里程碑 5：实时区域贴图**——目标窗口相对选区、GPU 裁剪和实时显示。
- [ ] **里程碑 6：产品化**——设置、快捷键配置、日志、安装包、便携包和兼容性测试。

完整设计见
[LandscapeCutter C++ 重构设计](docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md)。

## 环境要求

- Windows 10 1903 或更高版本，包括 Windows 11；
- x64 处理器和操作系统；
- Visual Studio 2026，安装 MSVC 14.5x C++ 工作负载；
- Windows SDK 10.0.19041 或更高版本；
- CMake 4.4 或更高版本；
- Git 和 PowerShell。

Qt 6.8.2、Catch2、spdlog 和 WIL 由固定版本的 vcpkg 引导脚本安装。目前不要求安装
Visual Studio 2022 Build Tools。

## 构建

在 PowerShell 中运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

首次执行引导脚本会完整克隆固定版本的 vcpkg 并安装依赖，因此需要网络连接和一定磁盘空间。
脚本会验证 vcpkg 标签、提交和工作树状态，避免静默使用错误的依赖版本。

## 测试

```powershell
ctest --preset windows-msvc-debug --output-on-failure
ctest --preset windows-msvc-debug-desktop --output-on-failure
```

默认 preset 排除需要当前交互桌面的 `desktop-integration`；desktop preset 只运行逐屏 Windows
Graphics Capture 与 session 关闭后 readback 验收。由于默认测试包含隔离的 vcpkg checkout 行为验证，
测试通常需要约 1–2 分钟。

## 运行

```powershell
& .\out\build\windows-msvc-debug\src\Debug\LandscapeCutter.exe
```

程序启动后驻留在 Windows 系统托盘，并显示启动通知。右键托盘图标，选择“退出”即可关闭。

按 F2 或选择托盘菜单“截图”进入冻结选区：

- 拖动空白处创建选区；拖动选区内部可移动，拖动边或角可缩放；
- Enter、Ctrl+C 或双击选区复制并结束；
- Ctrl+S 或“保存”可选择 PNG/JPEG 文件；
- Esc 或“取消”关闭本次截图。

选好区域后，工具栏提供矩形、椭圆、箭头、画笔、文字和马赛克。文字使用单击创建、Ctrl+Enter
提交、Esc 放弃当前编辑；马赛克块大小可设为 1–128 像素，默认 12。Ctrl+Z/Ctrl+Y 可撤销或重做，
导出的剪贴板和 PNG 保留 RGB32 像素，JPEG 使用质量 90。

## Python 原型

最终 Python 原型保存在带注释的 Git 标签 `python-prototype-final` 中。主分支只保留迁移说明，
不再维护 Python 运行版本。安全查看方式见 [legacy/README.md](legacy/README.md)。

## 开发文档

- [C++ 重构设计](docs/superpowers/specs/2026-09-02-cpp-rewrite-design.md)
- [里程碑 0 实施计划](docs/superpowers/plans/2026-09-02-milestone-0-cpp-foundation.md)
- [里程碑 0 执行进度](docs/superpowers/progress/2026-09-02-milestone-0-progress.md)
- [里程碑 1 执行进度](docs/superpowers/progress/2026-09-03-milestone-1-progress.md)
- [里程碑 2 详细设计](docs/superpowers/specs/2026-09-08-milestone-2-static-snip-design.md)
- [里程碑 2 实施计划](docs/superpowers/plans/2026-09-08-milestone-2-static-snip.md)
- [里程碑 2 执行进度](docs/superpowers/progress/2026-09-09-milestone-2-progress.md)
- [后续体验与外观优化计划](docs/superpowers/plans/2026-09-09-experience-and-appearance-backlog.md)

## 许可证

仓库目前还没有独立的许可证文件。在许可证确定并记录之前，请勿将构建产物作为正式版本重新
分发。
