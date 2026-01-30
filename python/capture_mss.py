#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter MSS屏幕捕获模块
"""

import mss
import numpy as np
import win32gui

class Capture:
    def __init__(self):
        """初始化捕获模块"""
        self.sct = mss.mss()
        self.target_window = None
        self.capture_method = "mss"
    
    def get_window_rect(self, hwnd):
        """获取窗口矩形
        
        Args:
            hwnd: 窗口句柄
            
        Returns:
            dict: 窗口矩形信息，包含 left, top, right, bottom
        """
        try:
            if hwnd:
                rect = win32gui.GetWindowRect(hwnd)
                return {
                    "left": rect[0],
                    "top": rect[1],
                    "right": rect[2],
                    "bottom": rect[3]
                }
            return None
        except Exception as e:
            print(f"获取窗口矩形错误: {str(e)}")
            return None
    
    def capture(self, region):
        """捕获指定区域
        
        Args:
            region: 区域信息，包含 x, y, width, height
            
        Returns:
            np.ndarray: 捕获的图像数据
        """
        try:
            if self.capture_method == "mss":
                # 计算实际捕获区域
                capture_region = region.copy()
                
                # 如果设置了目标窗口，基于窗口坐标进行捕获
                if self.target_window:
                    window_rect = self.get_window_rect(self.target_window)
                    if window_rect:
                        # 计算相对于窗口的坐标
                        capture_region["x"] = window_rect["left"] + region.get("relative_x", 0)
                        capture_region["y"] = window_rect["top"] + region.get("relative_y", 0)
                
                # 使用MSS捕获屏幕区域
                monitor = {
                    "top": capture_region["y"],
                    "left": capture_region["x"],
                    "width": capture_region["width"],
                    "height": capture_region["height"]
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
