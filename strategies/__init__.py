"""
策略模块
包含所有交易策略实现和基础类
"""

__version__ = "1.0.0"
__author__ = "Crypto Quant Team"
__description__ = "加密货币量化交易策略模块"

# 导出主要类和函数
from .config import (
    ConfigManager,
    SystemConfig,
    MarketDataConfig,
    StrategyConfig,
    ExecutionConfig,
    LoggingConfig,
    config_manager
)

from .strategy_base import (
    StrategyBase,
    Position,
    Trade
)

# 导出策略实现
from .mean_reversion_strategy import MeanReversionStrategy
from .momentum_strategy import MomentumStrategy
from .lstm_strategy import LSTMTradingStrategy, CryptoDataFetcher

__all__ = [
    'ConfigManager',
    'SystemConfig',
    'MarketDataConfig',
    'StrategyConfig',
    'ExecutionConfig',
    'LoggingConfig',
    'config_manager',
    'StrategyBase',
    'Position',
    'Trade',
    'MeanReversionStrategy',
    'MomentumStrategy',
    'LSTMTradingStrategy',
    'CryptoDataFetcher',
]
