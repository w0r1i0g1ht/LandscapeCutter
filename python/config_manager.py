#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
LandscapeCutter 配置管理器模块
"""

import json
import os

class ConfigMgr:
    def __init__(self, config_path=None):
        """初始化配置管理器
        
        Args:
            config_path: 配置文件路径
        """
        if config_path:
            self.config_path = config_path
        else:
            # 默认配置文件路径
            self.config_path = os.path.join(os.path.dirname(__file__), "..", "config.json")
    
    def save(self, config):
        """保存配置
        
        Args:
            config: 配置信息
            
        Returns:
            bool: 是否保存成功
        """
        try:
            # 确保配置文件目录存在
            os.makedirs(os.path.dirname(self.config_path), exist_ok=True)
            
            # 保存配置到JSON文件
            with open(self.config_path, 'w', encoding='utf-8') as f:
                json.dump(config, f, indent=2, ensure_ascii=False)
            return True
        except Exception as e:
            print(f"保存配置错误: {str(e)}")
            return False
    
    def load(self):
        """加载配置
        
        Returns:
            dict: 配置信息
        """
        try:
            if os.path.exists(self.config_path):
                with open(self.config_path, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                return config
            else:
                # 返回默认配置
                return self.get_default_config()
        except Exception as e:
            print(f"加载配置错误: {str(e)}")
            # 返回默认配置
            return self.get_default_config()
    
    def get_default_config(self):
        """获取默认配置
        
        Returns:
            dict: 默认配置信息
        """
        return {
            "version": "1.0",
            "capture": {
                "method": "mss",
                "region": {
                    "x": 960 - 100,  # 假设屏幕宽度1920
                    "y": 540 - 100,  # 假设屏幕高度1080
                    "width": 200,
                    "height": 200
                }
            },
            "target_window": None,
            "floating_window": {
                "position": {
                    "x": 100,
                    "y": 100
                },
                "size": {
                    "width": 200,
                    "height": 200
                },
                "opacity": 1.0,
                "always_on_top": True
            },
            "performance": {
                "fps": 30,
                "capture_interval": 33  # 毫秒
            }
        }
