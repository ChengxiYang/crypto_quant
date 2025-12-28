import sys
import numpy as np
import matplotlib.pyplot as plt

def simulate_gbm(S0, mu, sigma, T, dt, periods):
    n_steps = int(T / dt)
    # 预生成随机数
    Z = np.random.standard_normal((n_steps, periods))
    # 路径初始化
    S = np.zeros((n_steps + 1, periods))
    S[0] = S0
    
    # 向量化计算
    for t in range(1, n_steps + 1):
        S[t] = S[t-1] * np.exp((mu - 0.5 * sigma**2) * dt + sigma * np.sqrt(dt) * Z[t-1])
    return S

def main():
    # 参数设置
    S0, mu, sigma, T, dt = 100, 0.05, 0.2, 1, 1/252
    paths = simulate_gbm(S0, mu, sigma, T, dt, 5)

    plt.plot(paths)
    plt.title("Geometric Brownian Motion Paths")
    plt.show()

if __name__ == "__main__":
    sys.exit(main())