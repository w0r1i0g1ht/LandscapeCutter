LandscapeCutter 总体技术文档
1. 项目概述

项目名称：LandscapeCutter
项目定位：Windows 桌面工具，受哆啦A梦“风景刀”启发，实现任意应用窗口指定区域的实时裁剪显示，生成一个可拖拽、始终置顶的浮窗，方便在做其他事情时实时观察该区域界面。

目标：

个人生产力工具 / 学习操作系统图形栈

GitHub 项目经历展示

可拓展到多窗口、多区域或高性能后端

非目标：

输入事件转发（点击、键盘）

游戏/DRM窗口支持

跨平台（仅限 Windows）

系统级无延迟 (<16ms) 场景

2. 核心功能
2.1 当前 MVP 功能

Python + PySide6 实现 UI 与悬浮窗

mss 捕获屏幕或目标窗口区域

固定区域裁剪（200×200，默认屏幕中心）

悬浮窗置顶显示，实时刷新 (~30 fps)

可拖拽

2.2 后续可拓展功能

C++ DXGI 后端替换 Python 捕获，提高性能

支持选择任意窗口和区域

多浮窗并行显示

保存/恢复裁剪配置

GPU 共享纹理渲染，降低延迟

3. 系统架构
┌─────────────────────────────┐
│        Python UI 层          │
│ ┌────────────┐ ┌──────────┐ │
│ │ Main Window│ │ Floating │ │
│ │ (选择窗口) │ │ Window   │ │
│ └────────────┘ └──────────┘ │
└──────────────▲──────────────┘
               │ 用户操作 / 参数
               │
┌──────────────┴──────────────┐
│      Python / C++ 核心层     │
│ ┌────────────┐ ┌──────────┐ │
│ │ Capture    │ │ Region   │ │
│ │ (mss / DXGI)│ │ Selector│ │
│ └────────────┘ └──────────┘ │
│ ┌────────────┐ ┌──────────┐ │
│ │ RenderMgr  │ │ Interface│ │
│ │ (QImage)   │ │ pybind11 │ │
│ └────────────┘ └──────────┘ │
└──────────────▲──────────────┘
               │ 帧数据
┌──────────────┴──────────────┐
│      Windows 系统图形层      │
│ DXGI / DWM / Desktop Duplication │
└─────────────────────────────┘


说明：

Python 层负责 UI、悬浮窗、逻辑和用户交互

C++ 层（后期）负责高性能捕获和 GPU 裁剪

数据通过 pybind11 绑定在 Python 中显示

4. 模块设计
4.1 Python 模块
模块	功能	技术
main.py	程序入口	PySide6 QApplication
floating_window.py	置顶悬浮窗显示	PySide6 QLabel/QImage, QTimer
capture_mss.py	MVP 屏幕捕获	mss, numpy
region_selector.py	框选裁剪区域	PySide6 Overlay
window_picker.py	选择目标窗口	pywin32
utils.py	工具函数、坐标转换	Python 标准库
4.2 C++ 模块（计划）
模块	功能	技术
landscapecutter_core	捕获指定窗口帧	DXGI Desktop Duplication
shader_crop	GPU 裁剪指定区域	DirectX Shader
pybind11_binding	Python 调用接口	pybind11
dll_interface	DLL 对外接口	extern "C"
5. 数据流
5.1 当前 Python MVP
目标屏幕区域 (mss) 
   ↓
numpy array
   ↓
OpenCV (RGB 转换)
   ↓
QImage → QLabel
   ↓
Floating Window 显示 (置顶)

5.2 后期 C++ DXGI 后端
目标窗口 (DXGI)
   ↓
GPU Texture
   ↓
Shader 裁剪 Region
   ↓
CPU buffer
   ↓
Python 显示


关键点：

Python 显示逻辑不依赖捕获方式

后期可平滑切换高性能后端

6. 线程与性能设计

UI 线程：PySide6 QApplication，处理用户操作、窗口事件

Capture/Render 线程：

Python MVP：定时器刷新 (~30 fps)

后期 DXGI：独立线程 + GPU 处理

设计目标：

避免阻塞 UI

保证悬浮窗响应流畅

7. 技术栈
层级	技术	用途
UI	PySide6 / PyQt6	悬浮窗、框选 Overlay、窗口管理
捕获	mss	Python MVP 屏幕捕获
渲染	OpenCV / QImage	图像格式转换，显示
高性能	C++ / DXGI / DirectX	后期捕获和裁剪
绑定	pybind11	Python ↔ C++
其他	pywin32	窗口信息获取
8. MVP 功能清单
功能	状态	技术/备注
固定裁剪区域显示	✅	mss + PySide6
悬浮窗置顶	✅	PySide6
拖拽悬浮窗	✅	PySide6
选择目标窗口	⚠️	可用 pywin32 枚举，暂未框选
多窗口支持	❌	后期可加
高性能 GPU 后端	❌	DXGI 后期实现
输入事件转发	❌	不做
9. 目录结构
LandscapeCutter/
├─ README.md
├─ requirements.txt
├─ python/
│  ├─ main.py
│  ├─ capture_mss.py
│  ├─ floating_window.py
│  ├─ region_selector.py
│  └─ window_picker.py
├─ cpp/
│  └─ landscapecutter_core/ 
├─ assets/
└─ docs/
    └─ design.md

10. GitHub 项目价值说明

工程性：Python + C++ 架构，展示跨语言设计能力

系统理解：Windows 图形系统、DXGI/DWM 捕获、GPU 裁剪

可玩性：真实可用的桌面工具

可扩展性：未来可添加输入事件转发、多窗口、GPU共享纹理

展示价值：清晰 README + commit 历史、MVP 可运行

11. 未来扩展方向

C++ DXGI 后端：提高帧率和低延迟

框选任意窗口区域：动态 overlay + 鼠标选择

多浮窗支持：同时显示多个应用区域

配置保存：保存裁剪区域位置与大小

GPU 共享纹理渲染：减少 CPU 拷贝，优化性能

输入事件转发（高级功能）