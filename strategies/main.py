"""
主程序入口
"""
import sys
import os
import time
import signal
from typing import Optional

# 添加项目路径
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from strategies.config import ConfigManager, SystemConfig
from strategies.strategy_base import StrategyBase
from strategies.mean_reversion_strategy import MeanReversionStrategy
from strategies.momentum_strategy import MomentumStrategy


class CryptoQuantSystem:
    """加密货币量化交易系统"""
    
    def __init__(self, config_file: str = "config.json"):
        self.config_manager = ConfigManager(config_file)
        self.config = self.config_manager.get_config()
        self.strategy: Optional[StrategyBase] = None
        self.is_running = False
        
        # 注册信号处理器
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
    
    def _signal_handler(self, signum, frame):
        """信号处理器"""
        print(f"\n收到信号 {signum}，正在关闭系统...")
        self.stop()
    
    def _create_strategy(self) -> StrategyBase:
        """创建策略实例"""
        strategy_name = self.config.strategy_config.name
        
        if strategy_name == "mean_reversion_strategy":
            return MeanReversionStrategy(self.config.strategy_config)
        elif strategy_name == "momentum_strategy":
            return MomentumStrategy(self.config.strategy_config)
        else:
            raise ValueError(f"未知的策略: {strategy_name}")
    
    def start(self):
        """启动系统"""
        print("启动加密货币量化交易系统...")
        print(f"策略: {self.config.strategy_config.name}")
        print(f"交易对: {self.config.market_data_config.symbols}")
        print(f"交易模式: {'实盘' if not self.config.execution_config.testnet else '测试网'}")
        print(f"交易开关: {'开启' if self.config.strategy_config.enable_trading else '关闭'}")
        
        try:
            # 创建策略
            self.strategy = self._create_strategy()
            
            # 启动策略
            self.strategy.start()
            self.is_running = True
            
            print("系统启动成功！")
            print("按 Ctrl+C 停止系统")
            
            # 主循环
            self._main_loop()
            
        except Exception as e:
            print(f"启动失败: {e}")
            self.stop()
    
    def stop(self):
        """停止系统"""
        if not self.is_running:
            return
        
        print("正在停止系统...")
        
        if self.strategy:
            self.strategy.stop()
        
        self.is_running = False
        print("系统已停止")
    
    def _main_loop(self):
        """主循环"""
        last_stats_time = time.time()
        
        while self.is_running:
            try:
                # 每秒打印一次统计信息
                current_time = time.time()
                if current_time - last_stats_time >= 1.0:
                    self._print_stats()
                    last_stats_time = current_time
                
                # 这里应该处理市场数据和订单更新
                # 目前只是简单的等待
                time.sleep(0.1)
                
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"主循环错误: {e}")
                time.sleep(1)
    
    def _print_stats(self):
        """打印统计信息"""
        if not self.strategy:
            return
        
        stats = self.strategy.get_performance_stats()
        print(f"\r运行时间: {stats['running_time']:.0f}s | "
              f"总交易: {stats['total_trades']} | "
              f"胜率: {stats['win_rate']:.1%} | "
              f"总PnL: {stats['total_pnl']:.2f} | "
              f"最大回撤: {stats['max_drawdown']:.1%} | "
              f"当前持仓: {stats['current_positions']}", end="", flush=True)


def main():
    """主函数"""
    print("=" * 60)
    print("加密货币量化交易系统 v1.0.0")
    print("=" * 60)
    
    # 检查配置文件
    config_file = "config.json"
    if len(sys.argv) > 1:
        config_file = sys.argv[1]
    
    if not os.path.exists(config_file):
        print(f"配置文件不存在: {config_file}")
        print("请先创建配置文件或使用默认配置")
        return 1
    
    # 创建并启动系统
    system = CryptoQuantSystem(config_file)
    
    try:
        system.start()
    except KeyboardInterrupt:
        print("\n用户中断")
    except Exception as e:
        print(f"系统错误: {e}")
        return 1
    finally:
        system.stop()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
