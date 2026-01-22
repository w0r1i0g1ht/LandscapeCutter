#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 窗口选择器模块
"""

import win32gui
import win32process
import psutil
from PySide6.QtWidgets import QDialog, QVBoxLayout, QListWidget, QListWidgetItem, QPushButton, QLabel
from PySide6.QtCore import Qt

class WindowPicker:
    def __init__(self):
        """初始化窗口选择器"""
        pass
    
    def get_window_list(self):
        """获取窗口列表
        
        Returns:
            list: 窗口列表，每个元素包含窗口信息
        """
        windows = []
        
        def callback(hwnd, extra):
            """窗口回调函数"""
            if win32gui.IsWindowVisible(hwnd):
                title = win32gui.GetWindowText(hwnd)
                if title:
                    try:
                        # 获取窗口进程ID
                        _, process_id = win32process.GetWindowThreadProcessId(hwnd)
                        # 获取进程名称
                        process_name = ""
                        try:
                            process = psutil.Process(process_id)
                            process_name = process.name()
                        except:
                            pass
                        
                        windows.append({
                            "hwnd": hwnd,
                            "title": title,
                            "process_id": process_id,
                            "process_name": process_name
                        })
                    except Exception as e:
                        print(f"获取窗口信息错误: {str(e)}")
        
        # 枚举所有窗口
        win32gui.EnumWindows(callback, None)
        return windows
    
    def pick_window(self):
        """选择窗口
        
        Returns:
            dict: 选择的窗口信息，或None
        """
        # 获取窗口列表
        windows = self.get_window_list()
        
        # 创建窗口选择对话框
        dialog = QDialog()
        dialog.setWindowTitle("选择目标窗口")
        dialog.setGeometry(200, 200, 600, 400)
        
        layout = QVBoxLayout()
        
        # 标题
        label = QLabel("请选择要捕获的窗口:")
        layout.addWidget(label)
        
        # 窗口列表
        self.window_list = QListWidget()
        for window in windows:
            item = QListWidgetItem(f"{window['title']} ({window['process_name']})")
            item.setData(Qt.UserRole, window)
            self.window_list.addItem(item)
        layout.addWidget(self.window_list)
        
        # 按钮
        button_layout = QVBoxLayout()
        
        select_btn = QPushButton("选择")
        select_btn.clicked.connect(dialog.accept)
        button_layout.addWidget(select_btn)
        
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(dialog.reject)
        button_layout.addWidget(cancel_btn)
        
        layout.addLayout(button_layout)
        dialog.setLayout(layout)
        
        # 显示对话框
        if dialog.exec() == QDialog.Accepted:
            # 获取选择的窗口
            selected_items = self.window_list.selectedItems()
            if selected_items:
                return selected_items[0].data(Qt.UserRole)
        
        return None
