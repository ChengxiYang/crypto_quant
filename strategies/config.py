"""
配置管理模块
"""
import json
import os
from typing import Dict, Any, Optional
from dataclasses import dataclass, asdict


@dataclass
class MarketDataConfig:
    """市场数据配置"""
    exchange: str = "binance"
    symbols: list = None
    update_interval: float = 0.1  # 秒
    max_retries: int = 3
    timeout: float = 30.0
    
    def __post_init__(self):
        if self.symbols is None:
            self.symbols = ["BTCUSDT", "ETHUSDT"]


@dataclass
class StrategyConfig:
    """策略配置"""
    name: str = "default_strategy"
    risk_per_trade: float = 0.02  # 2%
    max_position_size: float = 1000.0
    stop_loss_pct: float = 0.05  # 5%
    take_profit_pct: float = 0.10  # 10%
    lookback_period: int = 100
    volatility_threshold: float = 0.02  # 2%
    enable_trading: bool = False  # 默认关闭交易


@dataclass
class ExecutionConfig:
    """执行配置"""
    exchange: str = "binance"
    api_key: str = ""
    secret_key: str = ""
    testnet: bool = True
    max_order_size: float = 1000.0
    max_daily_loss: float = 100.0
    max_orders_per_second: int = 10
    enable_risk_control: bool = True


@dataclass
class LoggingConfig:
    """日志配置"""
    level: str = "INFO"
    file_path: str = "logs/crypto_quant.log"
    max_file_size: int = 10 * 1024 * 1024  # 10MB
    backup_count: int = 5
    console_output: bool = True


@dataclass
class SystemConfig:
    """系统配置"""
    market_data_config: MarketDataConfig
    strategy_config: StrategyConfig
    execution_config: ExecutionConfig
    logging_config: LoggingConfig
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'SystemConfig':
        """从字典创建配置"""
        return cls(
            market_data_config=MarketDataConfig(**data.get('market_data', {})),
            strategy_config=StrategyConfig(**data.get('strategy', {})),
            execution_config=ExecutionConfig(**data.get('execution', {})),
            logging_config=LoggingConfig(**data.get('logging', {}))
        )
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'market_data': asdict(self.market_data_config),
            'strategy': asdict(self.strategy_config),
            'execution': asdict(self.execution_config),
            'logging': asdict(self.logging_config)
        }


class ConfigManager:
    """配置管理器"""
    
    def __init__(self, config_file: str = "config.json"):
        self.config_file = config_file
        self.config: Optional[SystemConfig] = None
        self._load_config()
    
    def _load_config(self):
        """加载配置文件"""
        if os.path.exists(self.config_file):
            try:
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                self.config = SystemConfig.from_dict(data)
            except Exception as e:
                print(f"加载配置文件失败: {e}")
                self._create_default_config()
        else:
            self._create_default_config()
    
    def _create_default_config(self):
        """创建默认配置"""
        self.config = SystemConfig(
            market_data_config=MarketDataConfig(),
            strategy_config=StrategyConfig(),
            execution_config=ExecutionConfig(),
            logging_config=LoggingConfig()
        )
        self.save_config()
    
    def save_config(self):
        """保存配置到文件"""
        try:
            os.makedirs(os.path.dirname(self.config_file), exist_ok=True)
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(self.config.to_dict(), f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"保存配置文件失败: {e}")
    
    def get_config(self) -> SystemConfig:
        """获取当前配置"""
        return self.config
    
    def update_config(self, **kwargs):
        """更新配置"""
        for key, value in kwargs.items():
            if hasattr(self.config, key):
                setattr(self.config, key, value)
        self.save_config()
    
    def reload_config(self):
        """重新加载配置"""
        self._load_config()


# 全局配置管理器实例
config_manager = ConfigManager()
