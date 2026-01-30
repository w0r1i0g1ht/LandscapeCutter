#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter MSS屏幕捕获模块
"""

import mss
import numpy as np

class Capture:
    def __init__(self):
        """初始化捕获模块"""
        self.sct = mss.mss()
        self.target_window = None
        self.capture_method = "mss"
    
    def capture(self, region):
        """捕获指定区域
        
        Args:
            region: 区域信息，包含 x, y, width, height
            
        Returns:
            np.ndarray: 捕获的图像数据
        """
        try:
            if self.capture_method == "mss":
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
            else:
                # 后续实现DXGI捕获
                raise NotImplementedError("DXGI capture not implemented yet")
        except Exception as e:
            print(f"捕获错误: {str(e)}")
            # 返回空图像
            return np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
    
    def set_target_window(self, hwnd):
        """设置目标窗口
        
        Args:
            hwnd: 窗口句柄
            
        Returns:
            bool: 是否设置成功
        """
        try:
            self.target_window = hwnd
            return True
        except Exception as e:
            print(f"设置目标窗口错误: {str(e)}")
            return False
    
    def set_capture_method(self, method):
        """设置捕获方法
        
        Args:
            method: 捕获方法，"mss" 或 "dxgi"
            
        Returns:
            bool: 是否设置成功
        """
        try:
            if method in ["mss", "dxgi"]:
                self.capture_method = method
                return True
            else:
                return False
        except Exception as e:
            print(f"设置捕获方法错误: {str(e)}")
            return False
