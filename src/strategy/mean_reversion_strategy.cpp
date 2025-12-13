#include "crypto_quant.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace crypto_quant {

// 均值回归策略实现
class MeanReversionStrategy : public IStrategy {
private:
    StrategyParams params_;
    StrategyStatus status_;
    std::vector<std::vector<double>> price_history_;
    std::vector<int> price_count_;
    mutable std::mutex mutex_;

public:
    MeanReversionStrategy() : status_(StrategyStatus::STOPPED) {
        price_history_.resize(3);
        price_count_.resize(3, 0);
        for (auto& history : price_history_) {
            history.reserve(100);
        }
    }

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = StrategyStatus::STOPPED;
        spdlog::info("MeanReversionStrategy initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& history : price_history_) {
            history.clear();
        }
        price_count_.assign(3, 0);
        status_ = StrategyStatus::STOPPED;
        spdlog::info("MeanReversionStrategy cleaned up");
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
        if (history.size() < static_cast<size_t>(params_.lookback_period)) {
            return SignalType::NONE;
        }

        // 计算移动平均
        double sum = 0.0;
        for (size_t i = history.size() - params_.lookback_period; i < history.size(); ++i) {
            sum += history[i];
        }
        double mean = sum / params_.lookback_period;

        // 计算标准差
        double variance = 0.0;
        for (size_t i = history.size() - params_.lookback_period; i < history.size(); ++i) {
            double diff = history[i] - mean;
            variance += diff * diff;
        }
        double std_dev = std::sqrt(variance / params_.lookback_period);

        // 计算Z-score
        double current_price = history.back();
        double z_score = (current_price - mean) / std_dev;

        // 生成交易信号
        if (z_score > params_.z_score_threshold) {
            spdlog::info("MeanReversionStrategy: SELL signal, z_score={:.2f}", z_score);
            return SignalType::SELL;
        } else if (z_score < -params_.z_score_threshold) {
            spdlog::info("MeanReversionStrategy: BUY signal, z_score={:.2f}", z_score);
            return SignalType::BUY;
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
        spdlog::info("MeanReversionStrategy parameters updated");
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

} // namespace crypto_quant