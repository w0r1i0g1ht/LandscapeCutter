#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 区域选择器模块
"""

from PySide6.QtWidgets import QDialog
from PySide6.QtCore import Qt, QPoint, QRect
from PySide6.QtGui import QPainter, QPen, QColor, QBrush

class RegionSelector(QDialog):
    def __init__(self, parent=None):
        """初始化区域选择器
        
        Args:
            parent: 父窗口
        """
        super().__init__(parent)
        
        # 设置窗口属性
        self.setWindowTitle("选择区域")
        self.setWindowFlags(Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setAttribute(Qt.WA_TranslucentBackground)
        
        # 获取屏幕大小
        from PySide6.QtWidgets import QApplication
        app = QApplication.instance()
        if not app:
            app = QApplication([])
        screen = app.primaryScreen()
        size = screen.size()
        
        # 设置窗口大小为整个屏幕
        self.setGeometry(0, 0, size.width(), size.height())
        
        # 选择区域相关
        self.start_point = QPoint()
        self.end_point = QPoint()
        self.selecting = False
        self.selected_region = None
    
    def paintEvent(self, event):
        """绘制事件"""
        painter = QPainter(self)
        
        # 绘制半透明背景
        overlay_color = QColor(0, 0, 0, 128)  # 半透明黑色
        painter.fillRect(self.rect(), overlay_color)
        
        # 绘制选择区域
        if self.selecting:
            # 计算选择区域
            rect = self.get_selection_rect()
            
            # 绘制选择区域的边框
            pen = QPen(QColor(255, 255, 255), 2, Qt.SolidLine)
            painter.setPen(pen)
            painter.drawRect(rect)
            
            # 绘制选择区域的背景（透明）
            transparent_brush = QBrush(QColor(0, 0, 0, 0))
            painter.setBrush(transparent_brush)
            painter.drawRect(rect)
            
            # 绘制选择区域的尺寸信息
            size_text = f"{rect.width()}x{rect.height()}"
            painter.setPen(QPen(QColor(255, 255, 255)))
            painter.drawText(rect.bottomRight() + QPoint(5, -5), size_text)
    
    def mousePressEvent(self, event):
        """鼠标按下事件"""
        if event.button() == Qt.LeftButton:
            self.selecting = True
            self.start_point = event.pos()
            self.end_point = event.pos()
            self.update()
    
    def mouseMoveEvent(self, event):
        """鼠标移动事件"""
        if self.selecting:
            self.end_point = event.pos()
            self.update()
    
    def mouseReleaseEvent(self, event):
        """鼠标释放事件"""
        if event.button() == Qt.LeftButton and self.selecting:
            self.selecting = False
            self.end_point = event.pos()
            
            # 计算选择区域
            rect = self.get_selection_rect()
            
            # 确保选择区域有效
            if rect.width() > 10 and rect.height() > 10:
                self.selected_region = {
                    "x": rect.x(),
                    "y": rect.y(),
                    "width": rect.width(),
                    "height": rect.height()
                }
                self.accept()
            else:
                # 选择区域太小，取消选择
                self.selected_region = None
                self.reject()
    
    def keyPressEvent(self, event):
        """键盘事件"""
        if event.key() == Qt.Key_Escape:
            # 按ESC取消选择
            self.selected_region = None
            self.reject()
    
    def get_selection_rect(self):
        """获取选择区域的矩形
        
        Returns:
            QRect: 选择区域的矩形
        """
        x1 = min(self.start_point.x(), self.end_point.x())
        y1 = min(self.start_point.y(), self.end_point.y())
        x2 = max(self.start_point.x(), self.end_point.x())
        y2 = max(self.start_point.y(), self.end_point.y())
        
        return QRect(x1, y1, x2 - x1, y2 - y1)
    
    def get_selected_region(self):
        """获取选择的区域
        
        Returns:
            dict: 选择的区域信息，包含 x, y, width, height
        """
        return self.selected_region
    
    def select_region(self):
        """选择区域
        
        Returns:
            dict: 选择的区域信息，或None
        """
        # 显示选择器
        if self.exec() == QDialog.Accepted:
            return self.selected_region
        else:
            return None
