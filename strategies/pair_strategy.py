import yfinance as yf
import pandas as pd
import numpy as np
import statsmodels.api as sm
from statsmodels.tsa.stattools import adfuller
import matplotlib.pyplot as plt

# ==========================================
# 1. 数据获取与预处理 (Data Acquisition)
# ==========================================
def get_data(tickers, start, end):
    print(f"正在获取 {tickers} 的数据...")
    data = yf.download(tickers, start, end, auto_adjust=True)['Close']
    # 丢弃空值
    data = data.dropna()
    return data

# ==========================================
# 2. 协整检验与价差构建 (Cointegration & Spread)
# ==========================================
def calculate_spread(df, asset_x, asset_y):
    # 取对数价格，符合金融物理学假设
    x = np.log(df[asset_x])
    y = np.log(df[asset_y])
    
    # OLS 回归: y = beta * x + alpha + epsilon
    # 注意：statsmodels 默认不加常数项，需手动添加
    X = sm.add_constant(x) 
    model = sm.OLS(y, X).fit()
    
    alpha = model.params['const']
    beta = model.params[asset_x]
    
    print(f"\n--- 回归结果 ---")
    print(f"对冲比率 (Beta): {beta:.4f}")
    print(f"这意味着每做空 1 元 {asset_y}，需买入 {beta:.4f} 元 {asset_x}")
    
    # 构建价差序列 (Spread = Residuals)
    # Spread = log(Y) - beta * log(X) - alpha
    spread = y - beta * x - alpha
    return spread, beta, alpha

# ==========================================
# 3. 均值回归动力学 (OU Process Calibration)
# ==========================================
def calculate_half_life(spread):
    # 我们将离散的价差序列拟合到 AR(1) 模型:
    # S(t) = const + phi * S(t-1) + epsilon
    
    spread_shift = spread.shift(1)
    spread_diff = spread - spread_shift
    spread_shift = spread_shift.dropna()
    spread_diff = spread_diff.dropna()
    
    # 回归: S(t) - S(t-1) 对 S(t-1) 回归 (这是拟合OU过程的一种简化方法)
    # 或者直接 S(t) 对 S(t-1) 回归。这里我们用 S(t) 对 S(t-1)
    
    X = sm.add_constant(spread_shift)
    model = sm.OLS(spread[1:], X).fit()
    
    phi = model.params.iloc[1] # 自回归系数
    
    # 根据 OU 过程推导：phi = exp(-kappa * dt)
    # 假设 dt = 1 (1天)
    # kappa = -ln(phi)
    kappa = -np.log(phi)
    
    # 半衰期 Half-Life = ln(2) / kappa
    if kappa <= 0:
        half_life = float('inf') # 如果 kappa < 0，说明发散，不是均值回归
    else:
        half_life = np.log(2) / kappa
        
    return half_life, kappa

# ==========================================
# 主程序执行
# ==========================================
if __name__ == "__main__":
    # 设定时间窗口：建议选择一段两者走势相对平稳的时期
    # 如果选最近的AI爆发期，协整关系可能已经破裂，这是很好的测试
    start_date = '2022-01-01'
    end_date = '2023-12-26' 
    t1, t2 = 'PDD', 'BABA'
    
    data = get_data([t1, t2], start_date, end_date)
    
    # 1. 计算价差
    spread, beta, alpha = calculate_spread(data, t1, t2)
    
    # 2. 检验平稳性 (ADF Test)
    adf = adfuller(spread)
    print(f"\n--- ADF 平稳性检验 ---")
    print(f"ADF Statistic: {adf[0]:.4f}")
    print(f"P-Value: {adf[1]:.4f}")
    if adf[1] < 0.05:
        print("结论: 序列平稳，存在协整关系，可以套利。")
    else:
        print("结论: 序列非平稳，协整关系弱，套利风险极大（纯赌博）。")

    # 3. 计算半衰期
    half_life, kappa = calculate_half_life(spread)
    print(f"\n--- OU 过程动力学 ---")
    print(f"均值回归速度 (Kappa): {kappa:.4f}")
    print(f"半衰期 (Half-Life): {half_life:.2f} 天")
    print("解读: 当价差偏离均值后，预计需要约 {:.1f} 天回归一半距离。".format(half_life))
    
    # 4. 生成交易信号 (Z-Score)
    # 使用滚动窗口计算均值和标准差，防止用到未来数据 (Look-ahead Bias)
    window = int(half_life) if half_life < 252 else 30 # 动态调整窗口
    if window < 5: window = 5
    
    spread_mean = spread.rolling(window=window).mean()
    spread_std = spread.rolling(window=window).std()
    z_score = (spread - spread_mean) / spread_std
    
    # 5. 绘图
    plt.figure(figsize=(14, 8))
    
    # 上图：原始价格
    plt.subplot(2, 1, 1)
    plt.plot(data[t1], label=t1)
    plt.plot(data[t2], label=t2)
    plt.title(f'{t1} vs {t2} Price History')
    plt.legend()
    plt.grid(True)
    
    # 下图：Z-Score 交易信号
    plt.subplot(2, 1, 2)
    plt.plot(z_score, label='Z-Score', color='blue', alpha=0.6)
    plt.axhline(2.0, color='red', linestyle='--', label='Short Spread (+2)')
    plt.axhline(-2.0, color='green', linestyle='--', label='Long Spread (-2)')
    plt.axhline(0, color='black', linewidth=1)
    plt.title(f'Pairs Trading Signal (Z-Score of Spread)')
    plt.legend()
    plt.grid(True)
    
    plt.tight_layout()
    plt.show()