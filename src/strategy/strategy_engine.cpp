#include "crypto_quant.h"
#include "market_data.h"  // 需要rest_client函数

// 前向声明rest_client函数
extern "C" {
    struct rest_client;
    struct rest_client* rest_client_create(const char* base_url);
    void rest_client_destroy(struct rest_client* client);
    int rest_client_get_orderbook(struct rest_client* client, symbol_t symbol, orderbook_t* orderbook);
}
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace crypto_quant {

// 策略引擎实现
class StrategyEngine : public IStrategyEngine {
private:
    std::shared_ptr<IStrategy> strategy_;
    std::atomic<bool> initialized_;
    std::atomic<StrategyStatus> status_;
    mutable std::mutex mutex_;

public:
    StrategyEngine() : initialized_(false), status_(StrategyStatus::STOPPED) {}

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_.store(true);
        spdlog::info("StrategyEngine initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_.load() != StrategyStatus::STOPPED) {
            stop();
        }
        initialized_.store(false);
        spdlog::info("StrategyEngine cleaned up");
    }

    void setStrategy(std::shared_ptr<IStrategy> strategy) override {
        std::lock_guard<std::mutex> lock(mutex_);
        strategy_ = strategy;
        spdlog::info("Strategy set in StrategyEngine");
    }

    void start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_.load()) {
            spdlog::error("StrategyEngine not initialized");
            return;
        }
        if (!strategy_) {
            spdlog::error("No strategy set in StrategyEngine");
            return;
        }
        status_.store(StrategyStatus::RUNNING);
        spdlog::info("StrategyEngine started");
    }

    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.store(StrategyStatus::STOPPED);
        spdlog::info("StrategyEngine stopped");
    }

    void pause() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.store(StrategyStatus::PAUSED);
        spdlog::info("StrategyEngine paused");
    }

    StrategyStatus getStatus() const override {
        return status_.load();
    }

    void processMarketData(const orderbook_t& orderbook) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_.load() == StrategyStatus::RUNNING && strategy_) {
            SignalType signal = strategy_->processMarketData(orderbook);
            spdlog::debug("Strategy processed market data, signal: {}", static_cast<int>(signal));
        }
    }
};

} // namespace crypto_quant
