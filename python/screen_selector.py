#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 屏幕选择器模块
类似Snipaste的桌面截取窗口
"""

from PySide6.QtWidgets import QDialog, QApplication
from PySide6.QtCore import Qt, QPoint, QRect
from PySide6.QtGui import QPainter, QPen, QColor, QBrush, QFont, QScreen

class ScreenSelector(QDialog):
    def __init__(self, parent=None):
        """初始化屏幕选择器
        
        Args:
            parent: 父窗口
        """
        super().__init__(parent)
        
        # 设置窗口属性
        self.setWindowTitle("选择区域")
        self.setWindowFlags(Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setAttribute(Qt.WA_TranslucentBackground)
        
        # 获取所有屏幕的几何信息
        app = QApplication.instance()
        if not app:
            app = QApplication([])
        
        # 计算所有屏幕的总边界
        total_rect = QRect()
        for screen in app.screens():
            total_rect = total_rect.united(screen.geometry())
        
        # 设置窗口大小为所有屏幕的总边界
        self.setGeometry(total_rect)
        
        # 保存屏幕信息用于坐标转换
        self.screens = app.screens()
        self.device_pixel_ratio = app.primaryScreen().devicePixelRatio()
        
        print(f"屏幕选择器几何信息: {self.geometry()}")
        print(f"设备像素比: {self.device_pixel_ratio}")
        
        # 选择区域相关
        self.start_point = QPoint()
        self.end_point = QPoint()
        self.selecting = False
        self.selected_region = None
        self.logical_rect = None
        
        # 鼠标位置
        self.current_mouse_pos = QPoint()
    
    def paintEvent(self, event):
        """绘制事件"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # 绘制半透明背景
        overlay_color = QColor(0, 0, 0, 128)
        painter.fillRect(self.rect(), overlay_color)
        
        # 绘制选择区域
        if self.selecting:
            # 计算选择区域
            rect = self.get_selection_rect()
            
            # 清除选择区域的背景（使其透明）
            painter.setCompositionMode(QPainter.CompositionMode_Clear)
            painter.fillRect(rect, Qt.transparent)
            painter.setCompositionMode(QPainter.CompositionMode_SourceOver)
            
            # 绘制选择区域的边框
            pen = QPen(QColor(255, 0, 0), 2, Qt.SolidLine)
            painter.setPen(pen)
            painter.drawRect(rect)
            
            # 绘制选择区域的尺寸信息
            size_text = f"{rect.width()} x {rect.height()}"
            font = QFont("Arial", 12)
            font.setBold(True)
            painter.setFont(font)
            
            # 计算文本位置
            text_rect = QRect(rect.x(), rect.y() - 30, 200, 30)
            painter.setPen(QPen(QColor(255, 255, 255)))
            painter.drawText(text_rect, Qt.AlignLeft, size_text)
            
            # 绘制坐标信息
            coord_text = f"({rect.x()}, {rect.y()})"
            coord_rect = QRect(rect.x(), rect.bottom() + 5, 200, 30)
            painter.drawText(coord_rect, Qt.AlignLeft, coord_text)
    
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
            self.current_mouse_pos = event.pos()
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
                # 获取全局坐标（相对于虚拟屏幕）
                global_x = self.geometry().x() + rect.x()
                global_y = self.geometry().y() + rect.y()
                
                # 保存逻辑矩形（用于设置悬浮窗位置和大小）
                self.logical_rect = {
                    "x": global_x,
                    "y": global_y,
                    "width": rect.width(),
                    "height": rect.height()
                }
                
                # 考虑DPI缩放，转换为物理像素坐标
                # 获取当前鼠标所在屏幕的DPI缩放比例
                app = QApplication.instance()
                cursor_pos = event.globalPos()
                
                # 找到鼠标所在的屏幕
                target_screen = None
                for screen in app.screens():
                    if screen.geometry().contains(cursor_pos):
                        target_screen = screen
                        break
                
                if target_screen is None:
                    target_screen = app.primaryScreen()
                
                # 获取该屏幕的DPI缩放比例
                dpr = target_screen.devicePixelRatio()
                
                # 计算物理像素坐标
                screen_geometry = target_screen.geometry()
                
                # 计算相对于屏幕的坐标
                rel_x = global_x - screen_geometry.x()
                rel_y = global_y - screen_geometry.y()
                
                # 转换为物理像素
                phys_x = int(screen_geometry.x() + rel_x * dpr)
                phys_y = int(screen_geometry.y() + rel_y * dpr)
                phys_width = int(rect.width() * dpr)
                phys_height = int(rect.height() * dpr)
                
                # 初始化区域信息（使用物理像素坐标）
                self.selected_region = {
                    "x": phys_x,
                    "y": phys_y,
                    "width": phys_width,
                    "height": phys_height
                }
                
                print(f"选择的区域（逻辑像素）: x={global_x}, y={global_y}, w={rect.width()}, h={rect.height()}")
                print(f"选择的区域（物理像素）: x={phys_x}, y={phys_y}, w={phys_width}, h={phys_height}")
                print(f"屏幕DPI缩放比例: {dpr}")
                
                self.accept()
            else:
                # 选择区域太小，取消选择
                self.selected_region = None
                self.logical_rect = None
                self.reject()
    
    def keyPressEvent(self, event):
        """键盘事件"""
        if event.key() == Qt.Key_Escape:
            # 按ESC取消选择
            self.selected_region = None
            self.logical_rect = None
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
    
    def select_region(self):
        """选择区域
        
        Returns:
            tuple: (region, logical_rect)
                region: 物理像素坐标，用于截图
                logical_rect: 逻辑像素坐标和尺寸，用于设置悬浮窗位置和大小
        """
        # 显示选择器
        if self.exec() == QDialog.Accepted:
            return self.selected_region, self.logical_rect
        else:
            return None, None
