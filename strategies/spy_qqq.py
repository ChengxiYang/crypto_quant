import yfinance as yf
import pandas as pd
import numpy as np
import sys
import matplotlib.pyplot as plt

import yfinance as yf
import pandas as pd
import numpy as np

# --- 策略逻辑函数 ---
def run_strategy(b0_pct, m, y, z, data):
    initial_capital = 10000.0
    # 仓位记录
    bond_val = initial_capital * b0_pct
    spy_val = initial_capital * (1 - b0_pct)
    qqq_val = 0
    
    # 必须确保数据中有对应的列，否则无法计算份额
    # 为防止除以0或空值，做简单的防错处理
    try:
        bond_start_price = data['BOND'].iloc[0]
        spy_start_price = data['SPY'].iloc[0]
    except:
        return 0.0 # 数据不足

    bond_shares = bond_val / bond_start_price
    spy_shares = spy_val / spy_start_price
    qqq_shares = 0
    
    entry_price_spy = 0 
    triggered_level = 0 
    frozen = False
    
    portfolio_values = []
    spy_high = spy_start_price
    
    for i in range(len(data)):
        p_spy = data['SPY'].iloc[i]
        p_qqq = data['QQQ'].iloc[i]
        p_bond = data['BOND'].iloc[i]
        
        # 更新每日市值
        current_bond_val = bond_shares * p_bond
        current_spy_val = spy_shares * p_spy
        current_qqq_val = qqq_shares * p_qqq
        total_val = current_bond_val + current_spy_val + current_qqq_val
        portfolio_values.append(total_val)
        
        if p_spy > spy_high:
            spy_high = p_spy
        
        drawdown = (spy_high - p_spy) / spy_high
        
        # 5. 崩盘熔断
        if drawdown > z:
            frozen = True
            continue 
            
        if frozen and drawdown <= z:
            frozen = False

        # 4. 回弹平仓 (QQQ -> SPY)
        if triggered_level > 0 and p_spy >= entry_price_spy:
            cash_from_qqq = qqq_shares * p_qqq
            qqq_shares = 0
            spy_shares += cash_from_qqq / p_spy
            triggered_level = 0
            entry_price_spy = 0
            continue

        # 2. 触发买入 (Bond -> QQQ)
        if triggered_level == 0 and drawdown > m:
            invest_amt = current_bond_val * y
            bond_shares -= invest_amt / p_bond
            qqq_shares += invest_amt / p_qqq
            triggered_level = 1
            entry_price_spy = p_spy 
            
        # 3. 触发二级买入
        elif triggered_level == 1 and drawdown > 2 * m:
            current_bond_val = bond_shares * p_bond
            invest_amt = current_bond_val * y
            bond_shares -= invest_amt / p_bond
            qqq_shares += invest_amt / p_qqq
            triggered_level = 2
            entry_price_spy = p_spy 

    if len(portfolio_values) == 0:
        return 0.0
    return (portfolio_values[-1] - initial_capital) / initial_capital


def main():
    try:
        start = "2015-01-01"
        end = "2022-12-31"
        print("下载每日收盘价数据: [%s - %s]" % (start, end))
        tickers = ["SPY", "QQQ", "IEF"] 
        
        # 修正1: 使用 auto_adjust=True，这样返回的 'Close' 就是复权收盘价，无需找 'Adj Close'
        raw_df = yf.download(tickers, start, end, auto_adjust=True)
        
        # 修正2: 处理多层索引 (MultiIndex) 问题
        # yfinance 新版下载多个股票时，列通常是 (PriceType, Ticker) 或 (Ticker, PriceType)
        # 我们只需要 Close 列
        
        df_clean = pd.DataFrame()
        
        # 检查 'Close' 是否在列的一级索引中
        if 'Close' in raw_df.columns:
            # 格式可能是 raw_df['Close']['SPY']
            temp_df = raw_df['Close']
        else:
            # 如果 auto_adjust=True 没生效，或者列结构不同，尝试直接读取
            # 这种情况下通常 raw_df 的列名直接就是 Ticker (老版本) 或者其他结构
            temp_df = raw_df

        # 逐个提取并将列重命名，确保 DataFrame 只有三列：SPY, QQQ, BOND
        # 这种写法最“笨”但也最稳，不依赖具体版本结构
        for t in tickers:
            if t in temp_df.columns:
                df_clean[t] = temp_df[t]
            else:
                # 尝试在多级索引中寻找
                # 有时列名是 ('Close', 'SPY')
                try:
                    df_clean[t] = raw_df.xs(t, axis=1, level=1)['Close']
                except:
                    try:
                        df_clean[t] = raw_df.xs(t, axis=1, level=0)['Close']
                    except:
                        print(f"警告: 无法找到代码为 {t} 的数据列")

        # 去除空值
        df_clean = df_clean.dropna()
        
        # 确保列名正确映射 (IEF -> BOND)
        if "IEF" in df_clean.columns:
            df_clean = df_clean.rename(columns={"IEF": "BOND"})
        
        # 检查数据是否为空
        if df_clean.empty:
            raise ValueError("下载的数据为空，请检查网络连接或股票代码。")

        print("数据准备完成，开始回测...")
        print(f"数据前5行预览:\n{df_clean.head()}")

        # --- 设置参数 ---
        b0 = 0.20   # 20% 债券
        m_val = 0.12 # 12% 触发阈值
        y_val = 0.80 # 80% 债券换仓比例
        z_val = 0.35 # 35% 熔断阈值

        ret = run_strategy(b0, m_val, y_val, z_val, df_clean)
        
        print("-" * 30)
        print(f"策略参数: B0={b0}, m={m_val}, y={y_val}, z={z_val}")
        print(f"时间区间[{start} - {end}]内策略总收益率: {ret*100:.2f}%")
        
        # 对比基准
        spy_ret = (df_clean['SPY'].iloc[-1] - df_clean['SPY'].iloc[0]) / df_clean['SPY'].iloc[0]
        print(f"同期 SPY 买入持有收益: {spy_ret*100:.2f}%")
        
        qqq_ret = (df_clean['QQQ'].iloc[-1] - df_clean['QQQ'].iloc[0]) / df_clean['QQQ'].iloc[0]
        print(f"同期 QQQ 买入持有收益: {qqq_ret*100:.2f}%")

    except Exception as e:
        import traceback
        print(f"依然报错: {e}")
        print("详细错误信息:")
        traceback.print_exc()



if __name__ == "__main__":
    sys.exit(main())