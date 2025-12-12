#!/usr/bin/env python3
"""
LSTM价格预测套利策略
使用LSTM神经网络预测虚拟货币短期价格走势，进行套利交易
整合了数据获取、模型训练、预测和回测功能
"""

import numpy as np
import pandas as pd
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from sklearn.preprocessing import MinMaxScaler, StandardScaler
from sklearn.metrics import mean_squared_error, mean_absolute_error
import logging
from typing import Dict, List, Tuple, Optional
import json
import os
import requests
import time
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

# 设置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


# ==================== 数据获取模块 ====================

class CryptoDataFetcher:
    """加密货币数据获取器"""
    
    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
        })
    
    def fetch_binance_data(self, symbol: str = "BTCUSDT", interval: str = "1h", 
                          limit: int = 1000) -> pd.DataFrame:
        """从币安获取K线数据"""
        try:
            url = "https://api.binance.com/api/v3/klines"
            params = {
                'symbol': symbol,
                'interval': interval,
                'limit': limit
            }
            
            response = self.session.get(url, params=params, timeout=10)
            response.raise_for_status()
            
            data = response.json()
            
            # 转换为DataFrame
            df = pd.DataFrame(data, columns=[
                'timestamp', 'open', 'high', 'low', 'close', 'volume',
                'close_time', 'quote_asset_volume', 'number_of_trades',
                'taker_buy_base_asset_volume', 'taker_buy_quote_asset_volume', 'ignore'
            ])
            
            # 数据类型转换
            df['timestamp'] = pd.to_datetime(df['timestamp'], unit='ms')
            numeric_columns = ['open', 'high', 'low', 'close', 'volume']
            for col in numeric_columns:
                df[col] = pd.to_numeric(df[col], errors='coerce')
            
            # 选择需要的列
            df = df[['timestamp', 'open', 'high', 'low', 'close', 'volume']].copy()
            df.set_index('timestamp', inplace=True)
            
            logger.info(f"成功获取 {symbol} {interval} 数据 {len(df)} 条")
            return df
            
        except Exception as e:
            logger.error(f"获取币安数据失败: {e}")
            return pd.DataFrame()
    
    def clean_data(self, df: pd.DataFrame) -> pd.DataFrame:
        """清理数据"""
        # 移除重复数据
        df = df.drop_duplicates()
        
        # 移除缺失值
        df = df.dropna()
        
        # 移除极端异常值（超过5个标准差）
        for column in ['open', 'high', 'low', 'close', 'volume']:
            if column in df.columns and len(df) > 0:
                mean_val = df[column].mean()
                std_val = df[column].std()
                if std_val > 0:
                    lower_bound = mean_val - 5 * std_val
                    upper_bound = mean_val + 5 * std_val
                    df = df[(df[column] >= lower_bound) & (df[column] <= upper_bound)]
        
        # 基本的价格逻辑检查
        if all(col in df.columns for col in ['open', 'high', 'low', 'close']) and len(df) > 0:
            df = df[df['high'] >= df['low']]
        
        logger.info(f"数据清理完成，剩余 {len(df)} 条记录")
        return df


# ==================== LSTM模型 ====================

class LSTMPricePredictor(nn.Module):
    """LSTM价格预测模型"""
    
    def __init__(self, input_size: int = 10, hidden_size: int = 64, 
                 num_layers: int = 2, output_size: int = 1, dropout: float = 0.2):
        super(LSTMPricePredictor, self).__init__()
        
        self.hidden_size = hidden_size
        self.num_layers = num_layers
        
        # LSTM层
        self.lstm = nn.LSTM(
            input_size=input_size,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=dropout if num_layers > 1 else 0
        )
        
        # 全连接层
        self.fc = nn.Sequential(
            nn.Linear(hidden_size, hidden_size // 2),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(hidden_size // 2, output_size)
        )
    
    def forward(self, x):
        # LSTM前向传播
        lstm_out, (hidden, cell) = self.lstm(x)
        
        # 取最后一个时间步的输出
        last_output = lstm_out[:, -1, :]
        
        # 全连接层
        output = self.fc(last_output)
        
        return output


class PriceDataset(Dataset):
    """价格数据集"""
    
    def __init__(self, data: np.ndarray, sequence_length: int = 60):
        self.data = data
        self.sequence_length = sequence_length
    
    def __len__(self):
        return len(self.data) - self.sequence_length
    
    def __getitem__(self, idx):
        sequence = self.data[idx:idx + self.sequence_length]
        target = self.data[idx + self.sequence_length]
        return torch.FloatTensor(sequence), torch.FloatTensor([target])


# ==================== LSTM交易策略 ====================

class LSTMTradingStrategy:
    """LSTM交易策略"""
    
    def __init__(self, config: Dict):
        self.config = config
        self.model = None
        self.scaler = MinMaxScaler()
        self.device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
        self.sequence_length = config.get('sequence_length', 60)
        self.prediction_horizon = config.get('prediction_horizon', 5)
        
        # 交易参数
        self.position_size = config.get('position_size', 0.1)
        self.stop_loss = config.get('stop_loss', 0.02)
        self.take_profit = config.get('take_profit', 0.03)
        self.confidence_threshold = config.get('confidence_threshold', 0.6)
        
        # 状态跟踪
        self.current_position = 0.0
        self.entry_price = 0.0
        self.last_prediction = None
        self.prediction_confidence = 0.0
        
        # 数据获取器
        self.data_fetcher = CryptoDataFetcher()
        
        logger.info(f"LSTM策略初始化完成，设备: {self.device}")
    
    def create_features(self, df: pd.DataFrame) -> np.ndarray:
        """创建特征矩阵"""
        features = []
        
        # 基础价格特征
        features.append(df['close'].values)
        features.append(df['volume'].values)
        
        # 移动平均线
        for window in [5, 10, 20, 50]:
            ma = df['close'].rolling(window=window).mean()
            features.append(ma.fillna(method='bfill').values)
        
        # RSI
        rsi = self.calculate_rsi(df['close'], 14)
        features.append(rsi.fillna(50).values)
        
        # MACD
        macd, macd_signal, macd_hist = self.calculate_macd(df['close'])
        features.append(macd.fillna(0).values)
        features.append(macd_signal.fillna(0).values)
        features.append(macd_hist.fillna(0).values)
        
        # 波动率
        volatility = df['close'].rolling(window=20).std()
        features.append(volatility.fillna(method='bfill').values)
        
        # 价格变化率
        price_change = df['close'].pct_change()
        features.append(price_change.fillna(0).values)
        
        # 组合特征矩阵
        feature_matrix = np.column_stack(features)
        
        # 处理无穷大和NaN值
        feature_matrix = np.nan_to_num(feature_matrix, nan=0.0, posinf=1e6, neginf=-1e6)
        
        # 标准化
        feature_matrix = self.scaler.fit_transform(feature_matrix)
        
        return feature_matrix
    
    def calculate_rsi(self, prices: pd.Series, window: int = 14) -> pd.Series:
        """计算RSI指标"""
        delta = prices.diff()
        gain = (delta.where(delta > 0, 0)).rolling(window=window).mean()
        loss = (-delta.where(delta < 0, 0)).rolling(window=window).mean()
        rs = gain / loss
        rsi = 100 - (100 / (1 + rs))
        return rsi
    
    def calculate_macd(self, prices: pd.Series, fast: int = 12, slow: int = 26, signal: int = 9) -> Tuple[pd.Series, pd.Series, pd.Series]:
        """计算MACD指标"""
        ema_fast = prices.ewm(span=fast).mean()
        ema_slow = prices.ewm(span=slow).mean()
        macd = ema_fast - ema_slow
        macd_signal = macd.ewm(span=signal).mean()
        macd_hist = macd - macd_signal
        return macd, macd_signal, macd_hist
    
    def prepare_data(self, df: pd.DataFrame) -> Tuple[DataLoader, DataLoader]:
        """准备训练和验证数据"""
        # 创建特征
        features = self.create_features(df)
        
        # 分割数据
        train_size = int(len(features) * 0.8)
        train_data = features[:train_size]
        val_data = features[train_size:]
        
        # 创建数据集
        train_dataset = PriceDataset(train_data, self.sequence_length)
        val_dataset = PriceDataset(val_data, self.sequence_length)
        
        # 创建数据加载器
        train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)
        val_loader = DataLoader(val_dataset, batch_size=32, shuffle=False)
        
        return train_loader, val_loader
    
    def train_model(self, train_loader: DataLoader, val_loader: DataLoader, 
                   epochs: int = 100, learning_rate: float = 0.001) -> Dict:
        """训练LSTM模型"""
        input_size = train_loader.dataset.data.shape[1]
        
        # 初始化模型
        self.model = LSTMPricePredictor(
            input_size=input_size,
            hidden_size=self.config.get('hidden_size', 64),
            num_layers=self.config.get('num_layers', 2),
            output_size=1,
            dropout=self.config.get('dropout', 0.2)
        ).to(self.device)
        
        # 优化器和损失函数
        optimizer = optim.Adam(self.model.parameters(), lr=learning_rate)
        criterion = nn.MSELoss()
        scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=10, factor=0.5)
        
        # 训练历史
        train_losses = []
        val_losses = []
        
        logger.info("开始训练LSTM模型...")
        
        for epoch in range(epochs):
            # 训练阶段
            self.model.train()
            train_loss = 0.0
            
            for batch_x, batch_y in train_loader:
                batch_x, batch_y = batch_x.to(self.device), batch_y.to(self.device)
                
                optimizer.zero_grad()
                outputs = self.model(batch_x)
                loss = criterion(outputs, batch_y)
                loss.backward()
                optimizer.step()
                
                train_loss += loss.item()
            
            # 验证阶段
            self.model.eval()
            val_loss = 0.0
            
            with torch.no_grad():
                for batch_x, batch_y in val_loader:
                    batch_x, batch_y = batch_x.to(self.device), batch_y.to(self.device)
                    outputs = self.model(batch_x)
                    loss = criterion(outputs, batch_y)
                    val_loss += loss.item()
            
            train_loss /= len(train_loader)
            val_loss /= len(val_loader)
            
            train_losses.append(train_loss)
            val_losses.append(val_loss)
            
            scheduler.step(val_loss)
            
            if epoch % 10 == 0:
                logger.info(f"Epoch {epoch}, Train Loss: {train_loss:.6f}, Val Loss: {val_loss:.6f}")
        
        logger.info("模型训练完成")
        
        return {
            'train_losses': train_losses,
            'val_losses': val_losses,
            'final_train_loss': train_losses[-1],
            'final_val_loss': val_losses[-1]
        }
    
    def predict_price(self, data: np.ndarray) -> Tuple[float, float]:
        """预测价格和置信度"""
        if self.model is None:
            raise ValueError("模型未训练，请先调用train_model方法")
        
        self.model.eval()
        
        with torch.no_grad():
            # 准备输入数据
            input_data = torch.FloatTensor(data[-self.sequence_length:]).unsqueeze(0).to(self.device)
            
            # 预测
            prediction = self.model(input_data).cpu().numpy()[0, 0]
            
            # 计算置信度
            confidence = min(1.0, max(0.0, 1.0 - abs(prediction - data[-1, 0]) / data[-1, 0]))
        
        return prediction, confidence
    
    def generate_trading_signal(self, current_price: float, prediction: float, 
                              confidence: float) -> Dict:
        """生成交易信号"""
        signal = {
            'action': 'HOLD',
            'confidence': confidence,
            'prediction': prediction,
            'current_price': current_price,
            'price_change_pct': (prediction - current_price) / current_price * 100,
            'timestamp': datetime.now().isoformat()
        }
        
        # 检查置信度阈值
        if confidence < self.confidence_threshold:
            return signal
        
        # 生成交易信号
        price_change_pct = (prediction - current_price) / current_price
        
        if price_change_pct > self.take_profit:
            signal['action'] = 'BUY'
            signal['reason'] = f"预测价格上涨 {price_change_pct:.2%}，置信度 {confidence:.2%}"
        elif price_change_pct < -self.take_profit:
            signal['action'] = 'SELL'
            signal['reason'] = f"预测价格下跌 {abs(price_change_pct):.2%}，置信度 {confidence:.2%}"
        
        return signal
    
    def execute_trade(self, signal: Dict, current_price: float) -> Dict:
        """执行交易"""
        trade_result = {
            'executed': False,
            'action': signal['action'],
            'price': current_price,
            'quantity': 0.0,
            'timestamp': datetime.now().isoformat()
        }
        
        if signal['action'] == 'HOLD':
            return trade_result
        
        # 计算交易数量
        if signal['action'] == 'BUY' and self.current_position <= 0:
            trade_result['quantity'] = self.position_size
            trade_result['executed'] = True
            self.current_position += self.position_size
            self.entry_price = current_price
            
        elif signal['action'] == 'SELL' and self.current_position >= 0:
            trade_result['quantity'] = -self.position_size
            trade_result['executed'] = True
            self.current_position -= self.position_size
            self.entry_price = current_price
        
        return trade_result
    
    def check_stop_loss_take_profit(self, current_price: float) -> Optional[Dict]:
        """检查止损止盈"""
        if self.current_position == 0:
            return None
        
        price_change = (current_price - self.entry_price) / self.entry_price
        
        # 检查止损
        if price_change <= -self.stop_loss:
            return {
                'action': 'STOP_LOSS',
                'price': current_price,
                'quantity': -self.current_position,
                'reason': f"触发止损，亏损 {abs(price_change):.2%}"
            }
        
        # 检查止盈
        if price_change >= self.take_profit:
            return {
                'action': 'TAKE_PROFIT',
                'price': current_price,
                'quantity': -self.current_position,
                'reason': f"触发止盈，盈利 {price_change:.2%}"
            }
        
        return None
    
    def backtest(self, df: pd.DataFrame, initial_capital: float = 10000.0) -> Dict:
        """回测策略"""
        logger.info("开始回测LSTM策略...")
        
        # 准备数据
        train_loader, val_loader = self.prepare_data(df)
        
        # 训练模型
        training_results = self.train_model(train_loader, val_loader)
        
        # 回测交易
        capital = initial_capital
        position = 0.0
        trades = []
        portfolio_values = []
        
        # 使用验证数据进行回测
        val_data = val_loader.dataset.data
        
        for i in range(self.sequence_length, len(val_data)):
            current_price = val_data[i, 0]
            
            # 预测价格
            try:
                prediction, confidence = self.predict_price(val_data[:i+1])
                
                # 生成交易信号
                signal = self.generate_trading_signal(current_price, prediction, confidence)
                
                # 执行交易
                if signal['action'] != 'HOLD':
                    trade = self.execute_trade(signal, current_price)
                    if trade['executed']:
                        trades.append(trade)
                        position += trade['quantity']
                        capital -= trade['quantity'] * current_price
                
                # 检查止损止盈
                stop_signal = self.check_stop_loss_take_profit(current_price)
                if stop_signal:
                    trades.append(stop_signal)
                    position += stop_signal['quantity']
                    capital -= stop_signal['quantity'] * current_price
                
                # 记录投资组合价值
                portfolio_value = capital + position * current_price
                portfolio_values.append(portfolio_value)
                
            except Exception as e:
                logger.warning(f"预测失败: {e}")
                continue
        
        # 计算回测结果
        if portfolio_values:
            total_return = (portfolio_values[-1] - initial_capital) / initial_capital
            max_drawdown = self.calculate_max_drawdown(portfolio_values)
            sharpe_ratio = self.calculate_sharpe_ratio(portfolio_values)
        else:
            total_return = 0.0
            max_drawdown = 0.0
            sharpe_ratio = 0.0
        
        backtest_results = {
            'initial_capital': initial_capital,
            'final_capital': portfolio_values[-1] if portfolio_values else initial_capital,
            'total_return': total_return,
            'max_drawdown': max_drawdown,
            'sharpe_ratio': sharpe_ratio,
            'total_trades': len(trades),
            'winning_trades': len([t for t in trades if t.get('quantity', 0) * (t['price'] - self.entry_price) > 0]),
            'training_results': training_results,
            'trades': trades[-10:]
        }
        
        logger.info(f"回测完成，总收益率: {total_return:.2%}")
        
        return backtest_results
    
    def calculate_max_drawdown(self, portfolio_values: List[float]) -> float:
        """计算最大回撤"""
        peak = portfolio_values[0]
        max_dd = 0.0
        
        for value in portfolio_values:
            if value > peak:
                peak = value
            dd = (peak - value) / peak
            if dd > max_dd:
                max_dd = dd
        
        return max_dd
    
    def calculate_sharpe_ratio(self, portfolio_values: List[float], risk_free_rate: float = 0.02) -> float:
        """计算夏普比率"""
        if len(portfolio_values) < 2:
            return 0.0
        
        returns = np.diff(portfolio_values) / portfolio_values[:-1]
        excess_returns = returns - risk_free_rate / 252
        
        if np.std(excess_returns) == 0:
            return 0.0
        
        return np.mean(excess_returns) / np.std(excess_returns) * np.sqrt(252)
    
    def save_model(self, filepath: str):
        """保存模型"""
        if self.model is None:
            raise ValueError("模型未训练")
        
        torch.save({
            'model_state_dict': self.model.state_dict(),
            'scaler': self.scaler,
            'config': self.config,
            'sequence_length': self.sequence_length
        }, filepath)
        
        logger.info(f"模型已保存到: {filepath}")
    
    def load_model(self, filepath: str):
        """加载模型"""
        checkpoint = torch.load(filepath, map_location=self.device)
        
        # 恢复配置
        self.config = checkpoint['config']
        self.sequence_length = checkpoint['sequence_length']
        
        # 恢复scaler
        self.scaler = checkpoint['scaler']
        
        # 重建模型
        input_size = len(self.scaler.feature_names_in_) if hasattr(self.scaler, 'feature_names_in_') else 10
        self.model = LSTMPricePredictor(
            input_size=input_size,
            hidden_size=self.config.get('hidden_size', 64),
            num_layers=self.config.get('num_layers', 2),
            output_size=1,
            dropout=self.config.get('dropout', 0.2)
        ).to(self.device)
        
        # 加载权重
        self.model.load_state_dict(checkpoint['model_state_dict'])
        self.model.eval()
        
        logger.info(f"模型已从 {filepath} 加载")
    
    def fetch_and_prepare_data(self, symbol: str = "BTCUSDT", interval: str = "1h", 
                              limit: int = 1000) -> pd.DataFrame:
        """获取并准备数据"""
        # 获取数据
        df = self.data_fetcher.fetch_binance_data(symbol, interval, limit)
        
        if df.empty:
            logger.error("数据获取失败")
            return df
        
        # 清理数据
        df = self.data_fetcher.clean_data(df)
        
        return df


def main():
    """主函数 - 示例用法"""
    # 配置参数
    config = {
        'sequence_length': 60,
        'prediction_horizon': 5,
        'hidden_size': 64,
        'num_layers': 2,
        'dropout': 0.2,
        'position_size': 0.1,
        'stop_loss': 0.02,
        'take_profit': 0.03,
        'confidence_threshold': 0.6
    }
    
    # 创建策略实例
    strategy = LSTMTradingStrategy(config)
    
    # 获取数据
    df = strategy.fetch_and_prepare_data("BTCUSDT", "1h", 1000)
    
    if not df.empty:
        # 回测
        results = strategy.backtest(df)
        logger.info(f"回测结果: {results}")
    else:
        logger.info("LSTM交易策略已初始化，请提供市场数据进行训练和回测")


if __name__ == "__main__":
    main()

