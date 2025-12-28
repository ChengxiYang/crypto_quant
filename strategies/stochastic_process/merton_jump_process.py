import sys
import numpy as np
import matplotlib.pyplot as plt

def simulate_merton_jump_diffusion(S0, mu, sigma, lamb, mu_j, sigma_j, T, dt, paths):
    n_steps = int(T / dt)
    t = np.linspace(0, T, n_steps + 1)
    
    # 预计算补偿项 kappa
    kappa = np.exp(mu_j + 0.5 * sigma_j**2) - 1
    
    # 初始化价格矩阵
    S = np.zeros((n_steps + 1, paths))
    S[0] = S0
    
    # 预生成随机数以提高效率
    Z = np.random.standard_normal((n_steps, paths))  # 连续部分的布朗运动
    # 模拟每个步长里的泊松跳跃次数
    N = np.random.poisson(lamb * dt, (n_steps, paths))
    
    for i in range(1, n_steps + 1):
        # 计算跳跃总和: 如果 N[i-1] > 0，则生成 N 次跳跃并求和
        jump_sum = np.zeros(paths)
        for j in range(paths):
            if N[i-1, j] > 0:
                # 对第 j 条路径，在该步长内发生的 N 次跳跃
                jump_sum[j] = np.random.normal(mu_j, sigma_j, N[i-1, j]).sum()
        
        # 价格更新公式
        drift = (mu - lamb * kappa - 0.5 * sigma**2) * dt
        diffusion = sigma * np.sqrt(dt) * Z[i-1]
        S[i] = S[i-1] * np.exp(drift + diffusion + jump_sum)
        
    return t, S


def main():
    # --- 参数设置 ---
    params = {
        "S0": 100,      # 初始价格
        "mu": 0.05,     # 无风险利率/期望收益
        "sigma": 0.2,   # 连续波动率
        "lamb": 0.75,   # 平均每年发生 0.75 次跳跃
        "mu_j": -0.2,   # 每次跳跃平均下跌 20%
        "sigma_j": 0.1, # 跳跃幅度的波动率
        "T": 1.0,       # 1年
        "dt": 1/252,    # 按交易日离散化
        "paths": 5      # 模拟5条路径
    }

    t, S_paths = simulate_merton_jump_diffusion(**params)

    # 可视化
    plt.figure(figsize=(10, 6))
    plt.plot(t, S_paths)
    plt.title(f"Merton Jump Diffusion Model ({params['paths']} Paths)")
    plt.xlabel("Time (Years)")
    plt.ylabel("Stock Price")
    plt.grid(True)
    plt.show()
    return 0

if __name__ == "__main__":
    sys.exit(main())