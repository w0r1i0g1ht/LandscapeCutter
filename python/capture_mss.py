#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter MSS屏幕捕获模块
"""

import mss
import numpy as np
import win32gui
import win32con
import win32ui
import ctypes

class Capture:
    def __init__(self):
        """初始化捕获模块"""
        self.sct = mss.mss()
        self.target_window = None
        self.capture_method = "mss"
        self.window_rect = None  # 目标窗口的位置和大小
        self.window_width = 0
        self.window_height = 0
        self.last_window_update = 0  # 上次更新窗口信息的时间戳
        self.update_interval = 100  # 窗口信息更新间隔（毫秒）
    
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
                if self.target_window:
                    # 检查目标窗口是否可见
                    if not win32gui.IsWindowVisible(self.target_window):
                        print("目标窗口不可见")
                        return np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
                    
                    # 检查目标窗口是否被其他窗口覆盖
                    foreground_hwnd = win32gui.GetForegroundWindow()
                    if foreground_hwnd != self.target_window:
                        # 当窗口不在前台时，使用PrintWindow方法捕获
                        return self.capture_with_printwindow(region)
                    
                    # 获取目标窗口的位置和大小
                    self.window_rect = win32gui.GetWindowRect(self.target_window)
                    window_x, window_y, window_right, window_bottom = self.window_rect
                    window_width = window_right - window_x
                    window_height = window_bottom - window_y
                    
                    # 计算相对于目标窗口的区域
                    actual_x = window_x + region["x"]
                    actual_y = window_y + region["y"]
                    actual_width = min(region["width"], window_width - region["x"])
                    actual_height = min(region["height"], window_height - region["y"])
                    
                    # 确保区域有效
                    if actual_width <= 0 or actual_height <= 0:
                        print("无效的捕获区域")
                        return np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
                else:
                    # 直接使用屏幕坐标
                    actual_x = region["x"]
                    actual_y = region["y"]
                    actual_width = region["width"]
                    actual_height = region["height"]
                
                # 使用MSS捕获屏幕区域
                monitor = {
                    "top": actual_y,
                    "left": actual_x,
                    "width": actual_width,
                    "height": actual_height
                }
                screenshot = self.sct.grab(monitor)
                # 转换为numpy数组
                img = np.array(screenshot)
                # 转换为RGB格式（MSS返回的是BGRA）
                img = img[:, :, :3]
                return img
            else:
                # 后续实现DXGI捕获
                raise NotImplementedError("DXGI capture not implemented yet")
        except Exception as e:
            print(f"捕获错误: {str(e)}")
            # 尝试使用PrintWindow作为备选方案
            if self.target_window:
                return self.capture_with_printwindow(region)
            # 返回空图像
            return np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
    
    def update_window_info(self):
        """更新窗口信息，减少频繁调用GetWindowRect的开销"""
        import time
        current_time = time.time() * 1000  # 转换为毫秒
        
        if current_time - self.last_window_update > self.update_interval:
            try:
                self.window_rect = win32gui.GetWindowRect(self.target_window)
                window_x, window_y, window_right, window_bottom = self.window_rect
                self.window_width = window_right - window_x
                self.window_height = window_bottom - window_y
                self.last_window_update = current_time
            except Exception as e:
                print(f"更新窗口信息错误: {str(e)}")
        
        return self.window_width, self.window_height
    
    def capture_with_printwindow(self, region):
        """使用PrintWindow API捕获窗口内容，即使窗口被覆盖
        
        Args:
            region: 区域信息，包含 x, y, width, height
            
        Returns:
            np.ndarray: 捕获的图像数据
        """
        try:
            # 获取目标窗口的位置和大小
            window_width, window_height = self.update_window_info()
            
            # 计算相对于目标窗口的区域
            actual_x = region["x"]
            actual_y = region["y"]
            actual_width = min(region["width"], window_width - region["x"])
            actual_height = min(region["height"], window_height - region["y"])
            
            # 确保区域有效
            if actual_width <= 0 or actual_height <= 0:
                print("无效的捕获区域")
                return np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
            
            # 创建设备上下文
            hwndDC = win32gui.GetWindowDC(self.target_window)
            mfcDC  = win32ui.CreateDCFromHandle(hwndDC)
            saveDC = mfcDC.CreateCompatibleDC()
            
            # 创建位图
            saveBitMap = win32ui.CreateBitmap()
            saveBitMap.CreateCompatibleBitmap(mfcDC, window_width, window_height)
            saveDC.SelectObject(saveBitMap)
            
            # 使用PrintWindow捕获窗口内容
            result = ctypes.windll.user32.PrintWindow(self.target_window, saveDC.GetSafeHdc(), 3)
            
            if result == 0:
                print("PrintWindow失败")
                # 清理资源
                saveDC.DeleteDC()
                mfcDC.DeleteDC()
                win32gui.ReleaseDC(self.target_window, hwndDC)
                win32gui.DeleteObject(saveBitMap.GetHandle())
                return np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
            
            # 获取位图信息
            bmpinfo = saveBitMap.GetInfo()
            bmpstr = saveBitMap.GetBitmapBits(True)
            
            # 转换为numpy数组
            img = np.frombuffer(bmpstr, dtype=np.uint8).reshape((bmpinfo['bmHeight'], bmpinfo['bmWidth'], 4))
            
            # 转换为RGB格式（去掉alpha通道）
            img = img[:, :, :3]
            
            # 裁剪到指定区域
            img = img[actual_y:actual_y+actual_height, actual_x:actual_x+actual_width]
            
            # 确保图像大小正确
            if img.shape[0] != region["height"] or img.shape[1] != region["width"]:
                # 如果大小不匹配，返回空图像
                img = np.zeros((region["height"], region["width"], 3), dtype=np.uint8)
            
            # 清理资源
            saveDC.DeleteDC()
            mfcDC.DeleteDC()
            win32gui.ReleaseDC(self.target_window, hwndDC)
            win32gui.DeleteObject(saveBitMap.GetHandle())
            
            return img
        except Exception as e:
            print(f"PrintWindow捕获错误: {str(e)}")
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
