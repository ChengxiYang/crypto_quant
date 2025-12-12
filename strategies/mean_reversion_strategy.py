"""
均值回归策略示例
"""
import numpy as np
from typing import Dict, Any, Optional
import time

from strategies.strategy_base import StrategyBase
from strategies.config import StrategyConfig


class MeanReversionStrategy(StrategyBase):
    """均值回归策略"""
    
    def __init__(self, config: StrategyConfig):
        super().__init__(config)
        self.price_history = {}  # 价格历史
        self.max_history = 100   # 最大历史长度
        self.z_score_threshold = 2.0  # Z分数阈值
        self.position_size = 0.1  # 仓位大小
        
    def on_market_data(self, orderbook):
        """处理市场数据"""
        if not self.is_running:
            return
        
        symbol = self._get_symbol_name(orderbook.symbol)
        mid_price = (orderbook.bids[0].price + orderbook.asks[0].price) / 2
        
        # 更新价格历史
        if symbol not in self.price_history:
            self.price_history[symbol] = []
        
        self.price_history[symbol].append(mid_price)
        if len(self.price_history[symbol]) > self.max_history:
            self.price_history[symbol].pop(0)
        
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
        
        if symbol not in self.price_history or len(self.price_history[symbol]) < 20:
            return None
        
        prices = np.array(self.price_history[symbol])
        mid_price = (orderbook.bids[0].price + orderbook.asks[0].price) / 2
        
        # 计算移动平均和标准差
        sma = np.mean(prices)
        std = np.std(prices)
        
        if std == 0:
            return None
        
        # 计算Z分数
        z_score = (mid_price - sma) / std
        
        # 生成信号
        if z_score > self.z_score_threshold:
            # 价格过高，卖出信号
            return {
                'type': 'sell',
                'symbol': symbol,
                'price': orderbook.bids[0].price,
                'quantity': self.position_size,
                'confidence': min(abs(z_score) / self.z_score_threshold, 1.0),
                'reason': f'价格过高，Z分数: {z_score:.2f}'
            }
        elif z_score < -self.z_score_threshold:
            # 价格过低，买入信号
            return {
                'type': 'buy',
                'symbol': symbol,
                'price': orderbook.asks[0].price,
                'quantity': self.position_size,
                'confidence': min(abs(z_score) / self.z_score_threshold, 1.0),
                'reason': f'价格过低，Z分数: {z_score:.2f}'
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
