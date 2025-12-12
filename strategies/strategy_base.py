"""
策略基类模块
"""
from abc import ABC, abstractmethod
from typing import Dict, Any, Optional
import time
from dataclasses import dataclass

from strategies.config import StrategyConfig


@dataclass
class Position:
    """持仓信息"""
    symbol: str
    side: str  # 'long' or 'short'
    size: float
    entry_price: float
    current_price: float
    unrealized_pnl: float
    timestamp: float


@dataclass
class Trade:
    """交易记录"""
    symbol: str
    side: str
    size: float
    price: float
    timestamp: float
    pnl: float = 0.0


class StrategyBase(ABC):
    """策略基类"""
    
    def __init__(self, config: StrategyConfig):
        self.config = config
        self.positions: Dict[str, Position] = {}
        self.trades: list[Trade] = []
        # self.is_running = False
        self.is_running = True
        self.start_time = None
        
        # 性能统计
        self.total_trades = 0
        self.winning_trades = 0
        self.losing_trades = 0
        self.total_pnl = 0.0
        self.max_drawdown = 0.0
        self.peak_equity = 0.0
    
    @abstractmethod
    def on_market_data(self, orderbook):
        """处理市场数据"""
        pass
    
    @abstractmethod
    def on_order_update(self, order):
        """处理订单更新"""
        pass
    
    @abstractmethod
    def generate_signal(self, orderbook) -> Optional[Dict[str, Any]]:
        """生成交易信号"""
        pass
    
    def start(self):
        """启动策略"""
        self.is_running = True
        self.start_time = time.time()
        print(f"策略 {self.config.name} 已启动")
    
    def stop(self):
        """停止策略"""
        self.is_running = False
        print(f"策略 {self.config.name} 已停止")
    
    def pause(self):
        """暂停策略"""
        self.is_running = False
        print(f"策略 {self.config.name} 已暂停")
    
    def resume(self):
        """恢复策略"""
        self.is_running = True
        print(f"策略 {self.config.name} 已恢复")
    
    def update_position(self, symbol: str, side: str, size: float, price: float):
        """更新持仓"""
        if symbol in self.positions:
            position = self.positions[symbol]
            if position.side == side:
                # 同方向加仓
                total_size = position.size + size
                position.entry_price = (position.entry_price * position.size + price * size) / total_size
                position.size = total_size
            else:
                # 反向减仓
                if size >= position.size:
                    # 平仓并反向开仓
                    self.close_position(symbol, position.size, price)
                    if size > position.size:
                        self.open_position(symbol, side, size - position.size, price)
                else:
                    # 部分平仓
                    self.close_position(symbol, size, price)
        else:
            # 新开仓
            self.open_position(symbol, side, size, price)
    
    def open_position(self, symbol: str, side: str, size: float, price: float):
        """开仓"""
        position = Position(
            symbol=symbol,
            side=side,
            size=size,
            entry_price=price,
            current_price=price,
            unrealized_pnl=0.0,
            timestamp=time.time()
        )
        self.positions[symbol] = position
        print(f"开仓: {symbol} {side} {size} @ {price}")
    
    def close_position(self, symbol: str, size: float, price: float):
        """平仓"""
        if symbol not in self.positions:
            return
        
        position = self.positions[symbol]
        pnl = (price - position.entry_price) * size if position.side == 'long' else (position.entry_price - price) * size
        
        # 记录交易
        trade = Trade(
            symbol=symbol,
            side=position.side,
            size=size,
            price=price,
            timestamp=time.time(),
            pnl=pnl
        )
        self.trades.append(trade)
        
        # 更新统计
        self.total_trades += 1
        self.total_pnl += pnl
        if pnl > 0:
            self.winning_trades += 1
        else:
            self.losing_trades += 1
        
        # 更新持仓
        position.size -= size
        if position.size <= 0:
            del self.positions[symbol]
        
        print(f"平仓: {symbol} {position.side} {size} @ {price}, PnL: {pnl:.2f}")
    
    def update_market_prices(self, prices: Dict[str, float]):
        """更新市场价格"""
        for symbol, price in prices.items():
            if symbol in self.positions:
                position = self.positions[symbol]
                position.current_price = price
                if position.side == 'long':
                    position.unrealized_pnl = (price - position.entry_price) * position.size
                else:
                    position.unrealized_pnl = (position.entry_price - price) * position.size
                
                # 更新最大回撤
                current_equity = self.total_pnl + sum(p.unrealized_pnl for p in self.positions.values())
                if current_equity > self.peak_equity:
                    self.peak_equity = current_equity
                else:
                    drawdown = (self.peak_equity - current_equity) / self.peak_equity if self.peak_equity > 0 else 0
                    if drawdown > self.max_drawdown:
                        self.max_drawdown = drawdown
    
    def get_performance_stats(self) -> Dict[str, Any]:
        """获取性能统计"""
        win_rate = self.winning_trades / self.total_trades if self.total_trades > 0 else 0
        avg_win = sum(t.pnl for t in self.trades if t.pnl > 0) / self.winning_trades if self.winning_trades > 0 else 0
        avg_loss = sum(t.pnl for t in self.trades if t.pnl < 0) / self.losing_trades if self.losing_trades > 0 else 0
        
        return {
            'total_trades': self.total_trades,
            'winning_trades': self.winning_trades,
            'losing_trades': self.losing_trades,
            'win_rate': win_rate,
            'total_pnl': self.total_pnl,
            'avg_win': avg_win,
            'avg_loss': avg_loss,
            'max_drawdown': self.max_drawdown,
            'current_positions': len(self.positions),
            'running_time': time.time() - self.start_time if self.start_time else 0
        }
    
    def risk_check(self, signal: Dict[str, Any]) -> bool:
        """风险检查"""
        if not self.config.enable_trading:
            return False
        
        # 检查最大持仓大小
        total_position_size = sum(abs(p.size) for p in self.positions.values())
        if total_position_size + signal.get('quantity', 0) > self.config.max_position_size:
            print(f"风险检查失败: 超过最大持仓大小限制")
            return False
        
        # 检查日亏损限制
        if self.total_pnl < -self.config.max_daily_loss:
            print(f"风险检查失败: 超过日亏损限制")
            return False
        
        return True
