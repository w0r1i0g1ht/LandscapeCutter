#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 屏幕捕获模块
"""

import mss
import numpy as np

class Capture:
    def __init__(self):
        """初始化捕获模块"""
        self.sct = mss.mss()
    
    def capture(self, region):
        """捕获指定区域
        
        Args:
            region: 区域信息，包含 x, y, width, height
            
        Returns:
            np.ndarray: 捕获的图像数据
        """
        try:
            # 使用MSS捕获屏幕区域
            monitor = {
                "top": region["y"],
                "left": region["x"],
                "width": region["width"],
                "height": region["height"]
            }
            screenshot = self.sct.grab(monitor)
            # 转换为numpy数组
            img = np.array(screenshot)
            # 转换为RGB格式（MSS返回的是BGRA，需要交换通道顺序）
            img = img[:, :, :3]
            # 交换B和R通道，从BGR转换为RGB
            img = img[:, :, ::-1]
            return img
        except Exception as e:
            print(f"捕获错误: {str(e)}")
            # 返回空图像
            return np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
