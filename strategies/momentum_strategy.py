"""
动量策略示例
"""
import numpy as np
from typing import Dict, Any, Optional
import time

from strategies.strategy_base import StrategyBase
from strategies.config import StrategyConfig


class MomentumStrategy(StrategyBase):
    """动量策略"""
    
    def __init__(self, config: StrategyConfig):
        super().__init__(config)
        self.price_history = {}  # 价格历史
        self.volume_history = {}  # 成交量历史
        self.max_history = 50    # 最大历史长度
        self.short_period = 10   # 短期周期
        self.long_period = 30    # 长期周期
        self.momentum_threshold = 0.02  # 动量阈值
        self.position_size = 0.1  # 仓位大小
        
    def on_market_data(self, orderbook):
        """处理市场数据"""
        if not self.is_running:
            return
        
        symbol = self._get_symbol_name(orderbook.symbol)
        mid_price = (orderbook.bids[0].price + orderbook.asks[0].price) / 2
        volume = sum(level.quantity for level in orderbook.bids[:5]) + \
                sum(level.quantity for level in orderbook.asks[:5])
        
        # 更新价格和成交量历史
        if symbol not in self.price_history:
            self.price_history[symbol] = []
            self.volume_history[symbol] = []
        
        self.price_history[symbol].append(mid_price)
        self.volume_history[symbol].append(volume)
        
        if len(self.price_history[symbol]) > self.max_history:
            self.price_history[symbol].pop(0)
            self.volume_history[symbol].pop(0)
        
        # 生成交易信号
        signal = self.generate_signal(orderbook)
        if signal and self.risk_check(signal):
            self._execute_signal(signal)
    
    def on_order_update(self, order):
        """处理订单更新"""
        print(f"订单更新: {order.order_id} - {order.status}")
    
    def generate_signal(self, orderbook) -> Optional[Dict[str, Any]]:
        """生成交易信号"""
        symbol = self._get_symbol_name(orderbook.symbol)
        
        if symbol not in self.price_history or len(self.price_history[symbol]) < self.long_period:
            return None
        
        prices = np.array(self.price_history[symbol])
        volumes = np.array(self.volume_history[symbol])
        mid_price = (orderbook.bids[0].price + orderbook.asks[0].price) / 2
        
        # 计算移动平均
        short_ma = np.mean(prices[-self.short_period:])
        long_ma = np.mean(prices[-self.long_period:])
        
        # 计算动量
        momentum = (short_ma - long_ma) / long_ma
        
        # 计算成交量加权价格
        vwap = np.sum(prices * volumes) / np.sum(volumes) if np.sum(volumes) > 0 else mid_price
        
        # 计算价格相对VWAP的位置
        price_vwap_ratio = (mid_price - vwap) / vwap
        
        # 生成信号
        if momentum > self.momentum_threshold and price_vwap_ratio > 0:
            # 向上动量，买入信号
            confidence = min(abs(momentum) / self.momentum_threshold, 1.0)
            return {
                'type': 'buy',
                'symbol': symbol,
                'price': orderbook.asks[0].price,
                'quantity': self.position_size,
                'confidence': confidence,
                'reason': f'向上动量，动量: {momentum:.4f}, 价格/VWAP: {price_vwap_ratio:.4f}'
            }
        elif momentum < -self.momentum_threshold and price_vwap_ratio < 0:
            # 向下动量，卖出信号
            confidence = min(abs(momentum) / self.momentum_threshold, 1.0)
            return {
                'type': 'sell',
                'symbol': symbol,
                'price': orderbook.bids[0].price,
                'quantity': self.position_size,
                'confidence': confidence,
                'reason': f'向下动量，动量: {momentum:.4f}, 价格/VWAP: {price_vwap_ratio:.4f}'
            }
        
        return None
    
    def _get_symbol_name(self, symbol_enum) -> str:
        """将枚举转换为字符串"""
        symbol_map = {
            0: "BTCUSDT",  # SYMBOL_BTC_USDT
            1: "ETHUSDT",  # SYMBOL_ETH_USDT
            2: "BTCETH"    # SYMBOL_BTC_ETH
        }
        return symbol_map.get(symbol_enum, "UNKNOWN")
    
    def _execute_signal(self, signal: Dict[str, Any]):
        """执行交易信号"""
        symbol = signal['symbol']
        side = signal['type']
        price = signal['price']
        quantity = signal['quantity']
        
        print(f"执行信号: {symbol} {side} {quantity} @ {price}")
        print(f"原因: {signal['reason']}")
        print(f"置信度: {signal['confidence']:.2f}")
        
        # 这里应该调用订单执行器
        # executor.submit_order(order)
        
        # 更新持仓（模拟）
        self.update_position(symbol, side, quantity, price)
