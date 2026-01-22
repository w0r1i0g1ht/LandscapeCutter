#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 调度器模块
"""

import threading
import time

class Scheduler:
    def __init__(self):
        """初始化调度器"""
        # 线程列表
        self.threads = []
        # 线程停止标志
        self.running = False
    
    def start_capture_thread(self, capture_func, interval=33):
        """启动捕获线程
        
        Args:
            capture_func: 捕获函数
            interval: 捕获间隔（毫秒）
        """
        self.running = True
        
        def capture_loop():
            """捕获循环"""
            while self.running:
                try:
                    capture_func()
                except Exception as e:
                    print(f"捕获线程错误: {str(e)}")
                time.sleep(interval / 1000.0)
        
        # 创建并启动线程
        thread = threading.Thread(target=capture_loop, daemon=True)
        thread.start()
        self.threads.append(thread)
    
    def start_render_thread(self, render_func, interval=33):
        """启动渲染线程
        
        Args:
            render_func: 渲染函数
            interval: 渲染间隔（毫秒）
        """
        self.running = True
        
        def render_loop():
            """渲染循环"""
            while self.running:
                try:
                    render_func()
                except Exception as e:
                    print(f"渲染线程错误: {str(e)}")
                time.sleep(interval / 1000.0)
        
        # 创建并启动线程
        thread = threading.Thread(target=render_loop, daemon=True)
        thread.start()
        self.threads.append(thread)
    
    def stop_threads(self):
        """停止所有线程"""
        self.running = False
        
        # 等待所有线程结束
        for thread in self.threads:
            if thread.is_alive():
                thread.join(timeout=1.0)
        
        # 清空线程列表
        self.threads.clear()
    
    def is_running(self):
        """检查调度器是否运行中
        
        Returns:
            bool: 是否运行中
        """
        return self.running
    
    def get_thread_count(self):
        """获取线程数量
        
        Returns:
            int: 线程数量
        """
        return len(self.threads)
