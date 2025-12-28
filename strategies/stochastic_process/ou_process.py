import sys
import numpy as np
from typing import Union
from numpy.typing import NDArray
import matplotlib.pyplot as plt

# def simulate_ou(x0, theta, mu, sigma, T, dt, periods):
def simulate_ou(
    x0: Union[float, NDArray[np.float64]],
    theta: float,
    mu: float,
    sigma: float,
    T: float,
    dt: float,
    periods: int
) -> NDArray[np.float64]:
    """
    模拟多条Ornstein-Uhlenbeck (OU) 过程路径（均值回归随机过程）
    
    参数说明：
        x0: OU过程初始值，标量（所有路径初始值相同）或长度为periods的一维数组（每条路径初始值不同）
        theta: 均值回归速度（θ），越大回归到mu的速度越快
        mu: 长期均值（μ），过程围绕该值波动
        sigma: 波动率（σ），控制随机波动幅度
        T: 模拟总时间长度
        dt: 离散时间步长（Δt）
        periods: 要模拟的独立OU路径数量
    
    返回值：
        二维numpy数组，形状为 (n_steps + 1, periods)
        - 行：时间点（0, dt, 2dt, ..., T）
        - 列：不同的OU路径
    """
    n_steps = int(T / dt)
    # 创建n_steps + 1行 & periods列的二维数组存储所有路径的结果，每一步一行
    x = np.zeros((n_steps + 1, periods))
    x[0] = x0
    
    for t in range(1, n_steps + 1):
        drift = theta * (mu - x[t-1]) * dt
        # 生成periods个独立的标准正态随机数
        diffusion = sigma * np.sqrt(dt) * np.random.standard_normal(periods)
        x[t] = x[t-1] + drift + diffusion
    return x

def main():
    # 参数：起始值0.5，回归均值1.0，回归速度2.0
    # ou_paths = simulate_ou(0.5, 2.0, 1.0, 0.1, 1, 1/252, 5)
    x0 = [0.5, -0.2, 0.9, 1.5, 0.7]
    ou_paths = simulate_ou(x0, 2.0, 1.0, 0.1, 2, 2/252, 5)
    plt.plot(ou_paths)
    plt.axhline(y=1.0, color='r', linestyle='--', label='Long-term Mean')
    plt.title("Ornstein-Uhlenbeck Process (Mean Reversion)")
    plt.legend()
    plt.show()
    return 0

if __name__ == "__main__":
    sys.exit(main())