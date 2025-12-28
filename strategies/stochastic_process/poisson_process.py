import sys
import numpy as np
import matplotlib.pyplot as plt

def simulate_poisson(lambda_rate, T, dt, periods):
    n_steps = int(T / dt)
    # 在每个dt时间内，判断是否发生跳跃 (0或1)
    # 概率 p = lambda * dt
    p = lambda_rate * dt
    jumps = np.random.binomial(1, p, (n_steps, periods))
    
    # 累计得到过程路径
    n_t = np.cumsum(jumps, axis=0)
    # 补上初始值0
    n_t = np.vstack([np.zeros(periods), n_t])
    return n_t

def main():
    # 参数：平均每年发生5次跳跃
    poisson_paths = simulate_poisson(5, 1, 1/252, 3)
    plt.step(np.linspace(0, 1, len(poisson_paths)), poisson_paths)
    plt.title("Poisson Process (Count of Events)")
    plt.show()
    return 0

if __name__ == "__main__":
    sys.exit(main())