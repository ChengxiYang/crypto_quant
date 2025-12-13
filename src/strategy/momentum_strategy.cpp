#include "crypto_quant.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace crypto_quant {

// 动量策略实现
class MomentumStrategy : public IStrategy {
private:
    StrategyParams params_;
    StrategyStatus status_;
    std::vector<std::vector<double>> price_history_;
    std::vector<int> price_count_;
    mutable std::mutex mutex_;

public:
    MomentumStrategy() : status_(StrategyStatus::STOPPED) {
        price_history_.resize(3);
        price_count_.resize(3, 0);
        for (auto& history : price_history_) {
            history.reserve(100);
        }
    }

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = StrategyStatus::STOPPED;
        spdlog::info("MomentumStrategy initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& history : price_history_) {
            history.clear();
        }
        price_count_.assign(3, 0);
        status_ = StrategyStatus::STOPPED;
        spdlog::info("MomentumStrategy cleaned up");
    }

    SignalType processMarketData(const orderbook_t& orderbook) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (status_ != StrategyStatus::RUNNING) {
            return SignalType::NONE;
        }

        int symbol_index = static_cast<int>(orderbook.symbol);
        if (symbol_index < 0 || symbol_index >= 3) {
            return SignalType::NONE;
        }

        // 更新价格历史
        double mid_price = (orderbook.bids[0].price + orderbook.asks[0].price) / 2.0;
        auto& history = price_history_[symbol_index];
        auto& count = price_count_[symbol_index];

        history.push_back(mid_price);
        if (history.size() > 100) {
            history.erase(history.begin());
        } else {
            count = static_cast<int>(history.size());
        }

        // 检查是否有足够的数据
        if (history.size() < static_cast<size_t>(params_.long_period)) {
            return SignalType::NONE;
        }

        // 计算短期和长期移动平均
        double short_sum = 0.0, long_sum = 0.0;
        
        for (int i = 0; i < params_.short_period; ++i) {
            short_sum += history[history.size() - 1 - i];
        }
        
        for (int i = 0; i < params_.long_period; ++i) {
            long_sum += history[history.size() - 1 - i];
        }
        
        double short_ma = short_sum / params_.short_period;
        double long_ma = long_sum / params_.long_period;
        
        // 计算动量
        double momentum = (short_ma - long_ma) / long_ma;
        
        // 生成交易信号
        if (momentum > params_.momentum_threshold) {
            spdlog::info("MomentumStrategy: BUY signal, momentum={:.4f}", momentum);
            return SignalType::BUY;
        } else if (momentum < -params_.momentum_threshold) {
            spdlog::info("MomentumStrategy: SELL signal, momentum={:.4f}", momentum);
            return SignalType::SELL;
        }

        return SignalType::NONE;
    }

    StrategyStatus getStatus() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    void setParams(const StrategyParams& params) override {
        std::lock_guard<std::mutex> lock(mutex_);
        params_ = params;
        spdlog::info("MomentumStrategy parameters updated");
    }

    StrategyParams getParams() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return params_;
    }

    void setStatus(StrategyStatus status) override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = status;
    }
};

}
