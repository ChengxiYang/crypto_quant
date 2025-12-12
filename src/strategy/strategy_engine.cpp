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

// RSI策略实现
class RSIStrategy : public IStrategy {
private:
    StrategyParams params_;
    StrategyStatus status_;
    std::vector<std::vector<double>> price_history_;
    std::vector<int> price_count_;
    mutable std::mutex mutex_;

    double calculateRSI(const std::vector<double>& prices, int period) {
        if (prices.size() < static_cast<size_t>(period + 1)) {
            return 50.0;  // 默认中性值
        }
        
        double gain_sum = 0.0, loss_sum = 0.0;
        
        for (size_t i = prices.size() - period; i < prices.size(); ++i) {
            double change = prices[i] - prices[i - 1];
            if (change > 0) {
                gain_sum += change;
            } else {
                loss_sum += std::abs(change);
            }
        }
        
        double avg_gain = gain_sum / period;
        double avg_loss = loss_sum / period;
        
        if (avg_loss == 0.0) {
            return 100.0;
        }
        
        double rs = avg_gain / avg_loss;
        return 100.0 - (100.0 / (1.0 + rs));
    }

public:
    RSIStrategy() : status_(StrategyStatus::STOPPED) {
        price_history_.resize(3);
        price_count_.resize(3, 0);
        for (auto& history : price_history_) {
            history.reserve(100);
        }
    }

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = StrategyStatus::STOPPED;
        spdlog::info("RSIStrategy initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& history : price_history_) {
            history.clear();
        }
        price_count_.assign(3, 0);
        status_ = StrategyStatus::STOPPED;
        spdlog::info("RSIStrategy cleaned up");
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
        if (history.size() < static_cast<size_t>(params_.rsi_period + 1)) {
            return SignalType::NONE;
        }

        // 计算RSI
        double rsi = calculateRSI(history, params_.rsi_period);
        
        // 生成交易信号
        if (rsi < params_.rsi_oversold) {
            spdlog::info("RSIStrategy: BUY signal, RSI={:.2f}", rsi);
            return SignalType::BUY;
        } else if (rsi > params_.rsi_overbought) {
            spdlog::info("RSIStrategy: SELL signal, RSI={:.2f}", rsi);
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
        spdlog::info("RSIStrategy parameters updated");
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

// 策略引擎实现
class StrategyEngine : public IStrategyEngine {
private:
    std::shared_ptr<IStrategy> strategy_;
    std::atomic<StrategyStatus> status_;
    mutable std::mutex mutex_;

public:
    StrategyEngine() : status_(StrategyStatus::STOPPED) {}

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = StrategyStatus::STOPPED;
        spdlog::info("StrategyEngine initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (strategy_) {
            strategy_->cleanup();
        }
        strategy_.reset();
        status_ = StrategyStatus::STOPPED;
        spdlog::info("StrategyEngine cleaned up");
    }

    void setStrategy(std::shared_ptr<IStrategy> strategy) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (strategy_) {
            strategy_->cleanup();
        }
        strategy_ = strategy;
        if (strategy_) {
            strategy_->initialize();
        }
        spdlog::info("Strategy set in StrategyEngine");
    }

    void start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (strategy_) {
            strategy_->setStatus(StrategyStatus::RUNNING);
        }
        status_ = StrategyStatus::RUNNING;
        spdlog::info("StrategyEngine started");
    }

    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (strategy_) {
            strategy_->setStatus(StrategyStatus::STOPPED);
        }
        status_ = StrategyStatus::STOPPED;
        spdlog::info("StrategyEngine stopped");
    }

    void pause() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (strategy_) {
            strategy_->setStatus(StrategyStatus::PAUSED);
        }
        status_ = StrategyStatus::PAUSED;
        spdlog::info("StrategyEngine paused");
    }

    StrategyStatus getStatus() const override {
        return status_.load();
    }

    void processMarketData(const orderbook_t& orderbook) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (strategy_ && status_ == StrategyStatus::RUNNING) {
            SignalType signal = strategy_->processMarketData(orderbook);
            if (signal != SignalType::NONE) {
                spdlog::info("StrategyEngine generated signal: {}", 
                           signal == SignalType::BUY ? "BUY" : 
                           signal == SignalType::SELL ? "SELL" : "HOLD");
            }
        }
    }
};

} // namespace crypto_quant