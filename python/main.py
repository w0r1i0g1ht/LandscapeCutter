#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 程序入口
"""

import sys
import win32gui
import win32api
import win32con
from PySide6.QtWidgets import QApplication, QMainWindow, QPushButton, QVBoxLayout, QWidget, QLabel
from PySide6.QtCore import Qt

from floating_window import FloatingWindow
from capture_mss import Capture
from window_picker import WindowPicker
from region_selector import RegionSelector
from config_manager import ConfigMgr

class MainWindow(QMainWindow):
    def __init__(self):
        """初始化主窗口"""
        super().__init__()
        self.setWindowTitle("LandscapeCutter")
        self.setGeometry(100, 100, 400, 300)
        
        # 初始化组件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        layout = QVBoxLayout()
        central_widget.setLayout(layout)
        
        # 标题
        title_label = QLabel("LandscapeCutter")
        title_label.setAlignment(Qt.AlignCenter)
        title_label.setStyleSheet("font-size: 18px; font-weight: bold;")
        layout.addWidget(title_label)
        
        # 选择窗口按钮
        self.select_window_btn = QPushButton("选择目标窗口")
        self.select_window_btn.clicked.connect(self.select_target_window)
        layout.addWidget(self.select_window_btn)
        
        # 选择区域按钮
        self.select_region_btn = QPushButton("选择区域")
        self.select_region_btn.clicked.connect(self.select_region)
        layout.addWidget(self.select_region_btn)
        
        # 创建悬浮窗按钮
        self.create_float_btn = QPushButton("创建悬浮窗")
        self.create_float_btn.clicked.connect(self.create_floating_window)
        layout.addWidget(self.create_float_btn)
        
        # 固定并后台运行按钮
        self.pin_background_btn = QPushButton("固定并后台运行")
        self.pin_background_btn.clicked.connect(self.pin_window_background)
        layout.addWidget(self.pin_background_btn)
        
        # 恢复窗口按钮
        self.restore_window_btn = QPushButton("恢复窗口")
        self.restore_window_btn.clicked.connect(self.restore_window)
        layout.addWidget(self.restore_window_btn)
        
        # 状态标签
        self.status_label = QLabel("就绪")
        self.status_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.status_label)
        
        # 初始化组件
        self.config_mgr = ConfigMgr()
        self.capture = Capture()
        self.floating_window = None
        self.target_window = None
        self.selected_region = None
        
        # 窗口状态管理
        self.window_state = "NORMAL"  # NORMAL 或 PSEUDO_MINIMIZED
        self.original_window_pos = None  # 原始窗口位置
        
        # 加载配置
        self.load_config()
    
    def select_target_window(self):
        """选择目标窗口"""
        try:
            window_picker = WindowPicker()
            self.target_window = window_picker.pick_window()
            if self.target_window:
                self.status_label.setText(f"已选择窗口: {self.target_window['title']}")
                self.capture.set_target_window(self.target_window['hwnd'])
                self.save_config()
            else:
                self.status_label.setText("未选择窗口")
        except Exception as e:
            self.status_label.setText(f"错误: {str(e)}")
    
    def select_region(self):
        """选择区域"""
        try:
            region_selector = RegionSelector(target_window=self.target_window)
            self.selected_region = region_selector.select_region()
            if self.selected_region:
                self.status_label.setText(f"已选择区域: {self.selected_region['width']}x{self.selected_region['height']}")
                self.save_config()
            else:
                self.status_label.setText("未选择区域")
        except Exception as e:
            self.status_label.setText(f"错误: {str(e)}")
    
    def create_floating_window(self):
        """创建悬浮窗"""
        try:
            if self.floating_window:
                self.floating_window.close()
            
            # 使用选择的区域或默认区域
            if self.selected_region:
                region = self.selected_region
            else:
                # 默认区域：屏幕中心 200x200
                region = {
                    "x": 960 - 100,  # 假设屏幕宽度1920
                    "y": 540 - 100,  # 假设屏幕高度1080
                    "width": 200,
                    "height": 200
                }
            
            self.floating_window = FloatingWindow(self.capture, region)
            self.floating_window.show()
            self.status_label.setText("悬浮窗已创建")
        except Exception as e:
            self.status_label.setText(f"错误: {str(e)}")
    
    def save_config(self):
        """保存配置"""
        config = {
            "target_window": self.target_window,
            "selected_region": self.selected_region,
            "capture": {
                "method": "mss"
            }
        }
        self.config_mgr.save(config)
    
    def pin_window_background(self):
        """固定窗口到后台运行（伪最小化）"""
        try:
            if not self.target_window:
                self.status_label.setText("请先选择目标窗口")
                return
            
            # 保存原始窗口位置
            hwnd = self.target_window['hwnd']
            rect = win32gui.GetWindowRect(hwnd)
            self.original_window_pos = {
                "left": rect[0],
                "top": rect[1],
                "right": rect[2],
                "bottom": rect[3]
            }
            
            # 将窗口移动到屏幕外
            screen_width = win32api.GetSystemMetrics(0)
            screen_height = win32api.GetSystemMetrics(1)
            
            # 移动到屏幕右下方外
            offscreen_x = screen_width + 1000
            offscreen_y = screen_height + 1000
            
            window_width = rect[2] - rect[0]
            window_height = rect[3] - rect[1]
            
            win32gui.SetWindowPos(
                hwnd,
                None,
                offscreen_x,
                offscreen_y,
                window_width,
                window_height,
                win32con.SWP_NOZORDER | win32con.SWP_NOACTIVATE
            )
            
            self.window_state = "PSEUDO_MINIMIZED"
            self.status_label.setText(f"窗口已固定到后台运行: {self.target_window['title']}")
        except Exception as e:
            self.status_label.setText(f"错误: {str(e)}")
    
    def restore_window(self):
        """恢复窗口显示"""
        try:
            if not self.target_window or not self.original_window_pos:
                self.status_label.setText("没有可恢复的窗口")
                return
            
            hwnd = self.target_window['hwnd']
            
            # 恢复窗口到原始位置
            win32gui.SetWindowPos(
                hwnd,
                None,
                self.original_window_pos["left"],
                self.original_window_pos["top"],
                self.original_window_pos["right"] - self.original_window_pos["left"],
                self.original_window_pos["bottom"] - self.original_window_pos["top"],
                win32con.SWP_NOZORDER
            )
            
            self.window_state = "NORMAL"
            self.status_label.setText(f"窗口已恢复: {self.target_window['title']}")
        except Exception as e:
            self.status_label.setText(f"错误: {str(e)}")
    
    def load_config(self):
        """加载配置"""
        config = self.config_mgr.load()
        if config:
            if "target_window" in config:
                self.target_window = config["target_window"]
                if self.target_window:
                    self.status_label.setText(f"已加载窗口: {self.target_window['title']}")
                    self.capture.set_target_window(self.target_window['hwnd'])
            if "selected_region" in config:
                self.selected_region = config["selected_region"]
                if self.selected_region:
                    self.status_label.setText(f"已加载区域: {self.selected_region['width']}x{self.selected_region['height']}")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
