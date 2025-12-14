#include "strategy_engine.h"
#include <spdlog/spdlog.h>

namespace crypto_quant {

StrategyEngine::StrategyEngine() : initialized_(false), status_(StrategyStatus::STOPPED) {
}

bool StrategyEngine::initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_.store(true);
        spdlog::info("StrategyEngine initialized");
        return true;
    }

void StrategyEngine::cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_.load() != StrategyStatus::STOPPED) {
            stop();
        }
        initialized_.store(false);
        spdlog::info("StrategyEngine cleaned up");
    }

void StrategyEngine::setStrategy(std::shared_ptr<IStrategy> strategy) {
        std::lock_guard<std::mutex> lock(mutex_);
        strategy_ = strategy;
        spdlog::info("Strategy set in StrategyEngine");
    }

void StrategyEngine::start() {
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

void StrategyEngine::stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.store(StrategyStatus::STOPPED);
        spdlog::info("StrategyEngine stopped");
    }

void StrategyEngine::pause() {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.store(StrategyStatus::PAUSED);
        spdlog::info("StrategyEngine paused");
    }

StrategyStatus StrategyEngine::getStatus() const {
        return status_.load();
    }

void StrategyEngine::processMarketData(const orderbook_t& orderbook) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_.load() == StrategyStatus::RUNNING && strategy_) {
            SignalType signal = strategy_->processMarketData(orderbook);
            spdlog::debug("Strategy processed market data, signal: {}", static_cast<int>(signal));
        }
    }
}
