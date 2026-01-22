#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 渲染管理器模块
"""

from PySide6.QtGui import QImage
import numpy as np

class RenderMgr:
    def __init__(self):
        """初始化渲染管理器"""
        # 帧缓存
        self.frame_buffer = None
    
    def convert_to_qimage(self, frame):
        """将捕获的帧转换为 QImage
        
        Args:
            frame: 图像数据，numpy 数组
            
        Returns:
            QImage: 转换后的 QImage
        """
        try:
            if frame is None or frame.size == 0:
                return None
            
            # 获取图像尺寸和通道数
            height, width, channels = frame.shape
            bytes_per_line = channels * width
            
            # 转换为QImage
            if channels == 3:
                # RGB格式
                q_image = QImage(frame.data, width, height, bytes_per_line, QImage.Format_RGB888)
            elif channels == 4:
                # RGBA格式
                q_image = QImage(frame.data, width, height, bytes_per_line, QImage.Format_RGBA8888)
            else:
                # 其他格式，暂不支持
                return None
            
            return q_image
        except Exception as e:
            print(f"转换为QImage错误: {str(e)}")
            return None
    
    def optimize_frame(self, frame):
        """优化帧数据以提高渲染性能
        
        Args:
            frame: 图像数据，numpy 数组
            
        Returns:
            np.ndarray: 优化后的图像数据
        """
        try:
            if frame is None or frame.size == 0:
                return frame
            
            # 这里可以添加一些优化，例如：
            # 1. 调整图像大小以适应显示
            # 2. 应用压缩
            # 3. 只更新变化的区域
            
            # 暂时直接返回原始帧
            return frame
        except Exception as e:
            print(f"优化帧错误: {str(e)}")
            return frame
    
    def set_frame_buffer(self, frame):
        """设置帧缓存
        
        Args:
            frame: 图像数据，numpy 数组
        """
        self.frame_buffer = frame
    
    def get_frame_buffer(self):
        """获取帧缓存
        
        Returns:
            np.ndarray: 帧缓存数据
        """
        return self.frame_buffer
