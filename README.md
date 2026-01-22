# LandscapeCutter

## 项目概述

LandscapeCutter是一个Windows桌面工具，受哆啦A梦"风景刀"启发，实现任意应用窗口指定区域的实时裁剪显示，生成一个可拖拽、始终置顶的浮窗，方便在做其他事情时实时观察该区域界面。

## 核心功能

### 当前MVP功能
- Python + PySide6实现UI与悬浮窗
- mss捕获屏幕或目标窗口区域
- 固定区域裁剪（200×200，默认屏幕中心）
- 悬浮窗置顶显示，实时刷新 (~30 fps)
- 可拖拽

### 后续可拓展功能
- C++ DXGI后端替换Python捕获，提高性能
- 支持选择任意窗口和区域
- 多浮窗并行显示
- 保存/恢复裁剪配置
- GPU共享纹理渲染，降低延迟

## 技术栈

- **编程语言**：Python 3.9+, C++17
- **UI框架**：PySide6 6.4+
- **屏幕捕获**：mss 7.0+ (MVP), DXGI (高性能)
- **图像处理**：OpenCV 4.5+
- **窗口管理**：pywin32 303+
- **绑定工具**：pybind11 2.10+

## 目录结构

```
LandscapeCutter/
├─ README.md
├─ requirements.txt
├─ python/
│  ├─ main.py           # 程序入口
│  ├─ capture_mss.py    # MSS屏幕捕获
│  ├─ floating_window.py # 悬浮窗显示
│  ├─ region_selector.py # 区域选择器
│  ├─ window_picker.py  # 窗口选择器
│  ├─ render_manager.py # 渲染管理器
│  ├─ config_manager.py # 配置管理器
│  ├─ scheduler.py      # 调度器
│  └─ utils.py          # 工具函数
├─ cpp/
│  └─ landscapecutter_core/ # C++核心模块
├─ assets/              # 资源文件
└─ docs/
    ├─ design.md        # 设计文档
    └─ technical_design.md # 技术设计文档
```

## 安装与运行

### 安装依赖

```bash
pip install -r requirements.txt
```

### 运行程序

```bash
python python/main.py
```

## 项目目标

- 个人生产力工具 / 学习操作系统图形栈
- GitHub项目经历展示
- 可拓展到多窗口、多区域或高性能后端

## 非目标

- 输入事件转发（点击、键盘）
- 游戏/DRM窗口支持
- 跨平台（仅限Windows）
- 系统级无延迟 (<16ms) 场景

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！
