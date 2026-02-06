#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 主程序
实现Alt+X快捷键截图和悬浮窗实时显示功能
"""

import sys
import os
import ctypes
from ctypes import wintypes
from PySide6.QtWidgets import QApplication, QWidget, QSystemTrayIcon, QMenu
from PySide6.QtCore import Qt, QTimer, Signal, QObject
from PySide6.QtGui import QAction, QIcon

from screen_selector import ScreenSelector
from floating_window import FloatingWindow
from capture_mss import Capture

# Windows API常量
WM_HOTKEY = 0x0312
MOD_ALT = 0x0001
MOD_CONTROL = 0x0002
MOD_SHIFT = 0x0004
MOD_WIN = 0x0008
VK_X = 0x58  # X键的虚拟键码

class HotkeyWindow(QWidget):
    """专门用于接收热键消息的窗口"""
    hotkey_pressed = Signal()
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("LandscapeCutter Hotkey")
        self.setGeometry(0, 0, 1, 1)
        self.setWindowFlags(Qt.Tool | Qt.FramelessWindowHint)
        self.hide()
        
        # 热键ID
        self.hotkey_id = 1
        
        # 注册热键
        self.register_hotkey()
        
        # 启动定时器检查热键消息
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.check_hotkey)
        self.timer.start(50)  # 每50ms检查一次
    
    def register_hotkey(self):
        """注册全局热键 Alt+X"""
        try:
            hwnd = int(self.winId())
            # 注册 Alt+X 热键
            if not ctypes.windll.user32.RegisterHotKey(hwnd, self.hotkey_id, MOD_ALT, VK_X):
                error_code = ctypes.windll.kernel32.GetLastError()
                print(f"注册全局热键失败，错误码: {error_code}")
                if error_code == 1409:
                    print("热键已被其他程序占用")
            else:
                print("全局 Alt+X 热键注册成功")
        except Exception as e:
            print(f"注册热键错误: {str(e)}")
    
    def check_hotkey(self):
        """检查热键消息"""
        try:
            hwnd = int(self.winId())
            msg = wintypes.MSG()
            
            # 检查是否有热键消息 (PM_REMOVE = 0x0001)
            while ctypes.windll.user32.PeekMessageW(ctypes.byref(msg), hwnd, WM_HOTKEY, WM_HOTKEY, 0x0001):
                if msg.message == WM_HOTKEY and msg.wParam == self.hotkey_id:
                    self.hotkey_pressed.emit()
        except Exception as e:
            pass
    
    def unregister_hotkey(self):
        """注销热键"""
        try:
            hwnd = int(self.winId())
            ctypes.windll.user32.UnregisterHotKey(hwnd, self.hotkey_id)
            print("全局热键已注销")
        except:
            pass

class MainWindow(QApplication):
    def __init__(self):
        """初始化主程序"""
        super().__init__(sys.argv)
        
        # 初始化捕获模块
        self.capture = Capture()
        
        # 当前悬浮窗
        self.floating_window = None
        
        # 创建热键窗口
        self.hotkey_window = HotkeyWindow()
        self.hotkey_window.hotkey_pressed.connect(self.start_screenshot)
        
        # 初始化系统托盘
        self.setup_tray()
        
        # 隐藏主窗口
        self.setQuitOnLastWindowClosed(False)
    
    def setup_tray(self):
        """设置系统托盘"""
        # 获取ico文件路径
        icon_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), "assets", "LandscapeCutter.ico")
        
        # 创建托盘图标
        self.tray_icon = QSystemTrayIcon(self)
        
        # 设置托盘图标
        if os.path.exists(icon_path):
            self.tray_icon.setIcon(QIcon(icon_path))
        else:
            self.tray_icon.setIcon(QIcon())
        
        # 创建托盘菜单
        tray_menu = QMenu()
        
        # 添加截图动作
        screenshot_action = QAction("截图 (Alt+X)", self)
        screenshot_action.triggered.connect(self.start_screenshot)
        tray_menu.addAction(screenshot_action)
        
        tray_menu.addSeparator()
        
        # 添加退出动作
        exit_action = QAction("退出", self)
        exit_action.triggered.connect(self.quit_app)
        tray_menu.addAction(exit_action)
        
        # 设置托盘菜单
        self.tray_icon.setContextMenu(tray_menu)
        
        # 连接托盘图标点击事件
        self.tray_icon.activated.connect(self.on_tray_activated)
        
        # 显示托盘图标
        self.tray_icon.show()
        
        # 显示提示信息
        self.tray_icon.showMessage(
            "LandscapeCutter",
            "按 Alt+X 开始截图\n双击悬浮窗可关闭\n点击托盘图标也可截图",
            QSystemTrayIcon.Information,
            3000
        )
    
    def on_tray_activated(self, reason):
        """托盘图标激活事件"""
        if reason in (QSystemTrayIcon.Trigger, QSystemTrayIcon.DoubleClick):
            self.start_screenshot()
    
    def start_screenshot(self):
        """开始截图"""
        try:
            # 如果已有悬浮窗，先关闭
            if self.floating_window:
                self.floating_window.close()
                self.floating_window = None
            
            # 创建屏幕选择器
            selector = ScreenSelector()
            
            # 显示选择器并获取选择的区域
            region, logical_rect = selector.select_region()
            
            if region and logical_rect:
                print(f"选择的区域（物理像素）: {region}")
                print(f"选择的区域（逻辑像素）: {logical_rect}")
                # 创建悬浮窗，传入逻辑矩形以确保窗口大小和位置与选择区域一致
                self.floating_window = FloatingWindow(self.capture, region, logical_rect)
                self.floating_window.show()
        except Exception as e:
            print(f"截图错误: {str(e)}")
            import traceback
            traceback.print_exc()
    
    def quit_app(self):
        """退出程序"""
        # 注销热键
        if self.hotkey_window:
            self.hotkey_window.unregister_hotkey()
            self.hotkey_window.close()
        
        if self.floating_window:
            self.floating_window.close()
        self.quit()

if __name__ == "__main__":
    app = MainWindow()
    sys.exit(app.exec())
