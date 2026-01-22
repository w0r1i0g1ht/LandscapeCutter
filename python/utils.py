#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 工具函数模块
"""

import os
import sys
import platform

class Utils:
    @staticmethod
    def get_screen_size():
        """获取屏幕尺寸
        
        Returns:
            tuple: (width, height)
        """
        try:
            from PySide6.QtWidgets import QApplication
            app = QApplication.instance()
            if not app:
                app = QApplication([])
            screen = app.primaryScreen()
            size = screen.size()
            return (size.width(), size.height())
        except Exception as e:
            print(f"获取屏幕尺寸错误: {str(e)}")
            # 返回默认值
            return (1920, 1080)
    
    @staticmethod
    def get_center_position(width, height):
        """获取屏幕中心位置
        
        Args:
            width: 窗口宽度
            height: 窗口高度
            
        Returns:
            tuple: (x, y)
        """
        screen_width, screen_height = Utils.get_screen_size()
        x = (screen_width - width) // 2
        y = (screen_height - height) // 2
        return (x, y)
    
    @staticmethod
    def is_windows():
        """检查是否为Windows系统
        
        Returns:
            bool: 是否为Windows系统
        """
        return platform.system() == "Windows"
    
    @staticmethod
    def get_resource_path(relative_path):
        """获取资源文件路径
        
        Args:
            relative_path: 相对路径
            
        Returns:
            str: 绝对路径
        """
        try:
            # PyInstaller 打包后的路径
            base_path = sys._MEIPASS
        except Exception:
            # 开发环境路径
            base_path = os.path.abspath(os.path.dirname(__file__))
        
        return os.path.join(base_path, relative_path)
    
    @staticmethod
    def clamp(value, min_value, max_value):
        """限制值在指定范围内
        
        Args:
            value: 输入值
            min_value: 最小值
            max_value: 最大值
            
        Returns:
            限制后的值
        """
        return max(min_value, min(value, max_value))
    
    @staticmethod
    def format_time(seconds):
        """格式化时间
        
        Args:
            seconds: 秒数
            
        Returns:
            str: 格式化后的时间字符串
        """
        minutes, seconds = divmod(seconds, 60)
        hours, minutes = divmod(minutes, 60)
        if hours > 0:
            return f"{hours:02d}:{minutes:02d}:{seconds:02d}"
        else:
            return f"{minutes:02d}:{seconds:02d}"
    
    @staticmethod
    def calculate_fps(elapsed_time, frame_count):
        """计算帧率
        
        Args:
            elapsed_time: 经过的时间（秒）
            frame_count: 帧数
            
        Returns:
            float: 帧率
        """
        if elapsed_time <= 0:
            return 0.0
        return frame_count / elapsed_time
