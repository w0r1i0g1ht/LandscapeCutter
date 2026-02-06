#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 悬浮窗模块
支持实时显示、拖动和双击关闭
"""

from PySide6.QtWidgets import QLabel
from PySide6.QtGui import QImage, QPixmap, QPainter, QColor, QPen
from PySide6.QtCore import Qt, QTimer, QPoint, QRectF

import numpy as np

class FloatingWindow(QLabel):
    def __init__(self, capture, region, logical_rect):
        """初始化悬浮窗
        
        Args:
            capture: 捕获对象
            region: 区域信息，包含 x, y, width, height（物理像素，用于截图）
            logical_rect: 逻辑矩形，包含 x, y, width, height（用于设置窗口位置和大小）
        """
        super().__init__()
        
        # 初始化参数
        self.capture = capture
        self.region = region
        self.logical_x = logical_rect["x"]
        self.logical_y = logical_rect["y"]
        self.logical_width = logical_rect["width"]
        self.logical_height = logical_rect["height"]
        self.border_color = QColor(255, 0, 0, 255)
        self.border_width = 2
        self.corner_radius = 0
        
        # 设置窗口属性
        self.setWindowTitle("LandscapeCutter")
        self.setWindowFlags(Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setAttribute(Qt.WA_TranslucentBackground)
        
        # 设置窗口大小为逻辑尺寸（与用户选择的矩形大小一致）
        self.setFixedSize(self.logical_width, self.logical_height)
        
        # 将窗口移动到选择区域的左上角位置（使用逻辑坐标）
        self.move(self.logical_x, self.logical_y)
        
        print(f"悬浮窗大小: {self.logical_width} x {self.logical_height}")
        print(f"悬浮窗位置: ({self.logical_x}, {self.logical_y})")
        
        # 拖拽相关
        self.dragging = False
        self.drag_position = QPoint()
        
        # 双击相关
        self.last_click_time = 0
        self.double_click_threshold = 300  # 毫秒
        self.click_count = 0
        
        # 初始化定时器，用于实时刷新
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update_frame)
        self.timer.start(16)  # ~60 fps，减少延迟到约16ms
    
    def paintEvent(self, event):
        """绘制事件 - 添加边框效果"""
        super().paintEvent(event)
        
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # 绘制边框
        rect = QRectF(0, 0, self.width(), self.height())
        pen = QPen(self.border_color, self.border_width)
        painter.setPen(pen)
        painter.setBrush(Qt.NoBrush)
        painter.drawRect(rect)
    
    def update_frame(self):
        """更新显示帧"""
        try:
            # 捕获区域（使用物理像素坐标）
            frame = self.capture.capture(self.region)
            if frame.size > 0:
                # 确保数组是C连续的
                frame = np.ascontiguousarray(frame)
                
                # 转换为QImage
                height, width, channels = frame.shape
                bytes_per_line = channels * width
                q_image = QImage(frame.data, width, height, bytes_per_line, QImage.Format_RGB888)
                
                # 将图像缩放到逻辑尺寸，保持与选择区域大小一致
                q_image = q_image.scaled(self.logical_width, self.logical_height, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                
                # 转换为QPixmap并显示
                pixmap = QPixmap.fromImage(q_image)
                self.setPixmap(pixmap)
        except Exception as e:
            print(f"更新帧错误: {str(e)}")
    
    def mousePressEvent(self, event):
        """鼠标按下事件"""
        if event.button() == Qt.LeftButton:
            import time
            current_time = int(time.time() * 1000)
            
            # 检查是否为双击
            if current_time - self.last_click_time < self.double_click_threshold:
                # 双击关闭
                self.close()
                return
            
            self.last_click_time = current_time
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
    
    def closeEvent(self, event):
        """关闭事件"""
        # 停止定时器
        self.timer.stop()
        event.accept()
