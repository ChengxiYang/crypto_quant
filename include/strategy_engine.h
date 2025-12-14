#ifndef STRATEGY_ENGINE_H
#define STRATEGY_ENGINE_H

#include "crypto_quant.h"
#include <memory>
#include <atomic>
#include <mutex>

namespace crypto_quant {

// 策略引擎实现类
class StrategyEngine : public IStrategyEngine {
private:
    std::shared_ptr<IStrategy> strategy_;
    std::atomic<bool> initialized_;
    std::atomic<StrategyStatus> status_;
    mutable std::mutex mutex_;

public:
    StrategyEngine();

    bool initialize() override;
    void cleanup() override;
    void setStrategy(std::shared_ptr<IStrategy> strategy) override;
    void start() override;
    void stop() override;
    void pause() override;
    StrategyStatus getStatus() const override;
    void processMarketData(const orderbook_t& orderbook) override;
};

} // namespace crypto_quant

#endif // STRATEGY_ENGINE_H

