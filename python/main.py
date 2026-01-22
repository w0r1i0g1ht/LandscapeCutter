#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 程序入口
"""

import sys
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
            # 传递目标窗口句柄给区域选择器
            target_window_hwnd = self.target_window['hwnd'] if self.target_window else None
            region_selector = RegionSelector(target_window=target_window_hwnd)
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
