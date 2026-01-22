#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 悬浮窗模块
"""

from PySide6.QtWidgets import QLabel, QWidget, QMenu
from PySide6.QtGui import QAction, QImage, QPixmap
from PySide6.QtCore import Qt, QTimer, QPoint

import numpy as np

class FloatingWindow(QLabel):
    def __init__(self, capture, region):
        """初始化悬浮窗
        
        Args:
            capture: 捕获对象
            region: 区域信息，包含 x, y, width, height
        """
        super().__init__()
        
        # 初始化参数
        self.capture = capture
        self.region = region
        
        # 设置窗口属性
        self.setWindowTitle("LandscapeCutter 悬浮窗")
        self.setWindowFlags(Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setAttribute(Qt.WA_TranslucentBackground)
        
        # 设置窗口大小和位置
        self.setGeometry(100, 100, region["width"], region["height"])
        
        # 拖拽相关
        self.dragging = False
        self.drag_position = QPoint()
        
        # 初始化定时器，用于实时刷新
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update_frame)
        self.timer.start(33)  # ~30 fps
    
    def update_frame(self):
        """更新显示帧"""
        try:
            # 捕获区域
            frame = self.capture.capture(self.region)
            if frame.size > 0:
                # 确保数组是C连续的
                import numpy as np
                frame = np.ascontiguousarray(frame)
                # 转换为QImage
                height, width, channels = frame.shape
                bytes_per_line = channels * width
                q_image = QImage(frame.data, width, height, bytes_per_line, QImage.Format_RGB888)
                # 转换为QPixmap并显示
                pixmap = QPixmap.fromImage(q_image)
                self.setPixmap(pixmap)
                self.setFixedSize(width, height)
        except Exception as e:
            print(f"更新帧错误: {str(e)}")
    
    def set_position(self, x, y):
        """设置位置
        
        Args:
            x: x 坐标
            y: y 坐标
        """
        self.move(x, y)
    
    def set_size(self, width, height):
        """设置大小
        
        Args:
            width: 宽度
            height: 高度
        """
        self.setFixedSize(width, height)
        self.region["width"] = width
        self.region["height"] = height
    
    def set_opacity(self, opacity):
        """设置透明度
        
        Args:
            opacity: 透明度，范围 0.0-1.0
        """
        self.setWindowOpacity(opacity)
    
    def start_dragging(self):
        """开始拖拽"""
        self.dragging = True
    
    def stop_dragging(self):
        """停止拖拽"""
        self.dragging = False
    
    # 鼠标事件处理
    def mousePressEvent(self, event):
        """鼠标按下事件"""
        if event.button() == Qt.LeftButton:
            self.dragging = True
            self.drag_position = event.globalPos() - self.frameGeometry().topLeft()
            event.accept()
    
    def mouseMoveEvent(self, event):
        """鼠标移动事件"""
        if event.buttons() == Qt.LeftButton and self.dragging:
            self.move(event.globalPos() - self.drag_position)
            event.accept()
    
    def mouseReleaseEvent(self, event):
        """鼠标释放事件"""
        if event.button() == Qt.LeftButton:
            self.dragging = False
            event.accept()
    
    def contextMenuEvent(self, event):
        """右键菜单事件"""
        # 创建右键菜单
        menu = QMenu(self)
        
        # 添加关闭动作
        close_action = QAction("关闭", self)
        close_action.triggered.connect(self.close)
        menu.addAction(close_action)
        
        # 显示菜单
        menu.exec(event.globalPos())
    
    def closeEvent(self, event):
        """关闭事件"""
        # 停止定时器
        self.timer.stop()
        event.accept()
