#include "crypto_quant.h"
#include "strategy/strategy_engine_impl.cpp"

namespace crypto_quant {

// 工厂类实现
std::shared_ptr<IStrategyEngine> CryptoQuantFactory::createStrategyEngine() {
    return std::shared_ptr<IStrategyEngine>(new StrategyEngine());
}

std::shared_ptr<IOrderExecutor> CryptoQuantFactory::createOrderExecutor() {
    return std::shared_ptr<IOrderExecutor>(new OrderExecutor());
}

std::shared_ptr<IOrderbookManager> CryptoQuantFactory::createOrderbookManager() {
    return std::shared_ptr<IOrderbookManager>(new OrderbookManager());
}

std::shared_ptr<IMarketDataProvider> CryptoQuantFactory::createMarketDataProvider() {
    return std::shared_ptr<IMarketDataProvider>(new MarketDataProvider());
}

std::shared_ptr<IStrategy> CryptoQuantFactory::createMeanReversionStrategy() {
    // 暂时返回nullptr，需要实现具体策略
    return nullptr;
}

std::shared_ptr<IStrategy> CryptoQuantFactory::createMomentumStrategy() {
    // 暂时返回nullptr，需要实现具体策略
    return nullptr;
}

std::shared_ptr<IStrategy> CryptoQuantFactory::createRSIStrategy() {
    // 暂时返回nullptr，需要实现具体策略
    return nullptr;
}

} // namespace crypto_quant
