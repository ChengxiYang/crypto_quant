#include <mutex>
#include "crypto_quant.h"
#include "orderbook.h"
#include "strategy.h"
#include "execution.h"
#include "market_data.h"

namespace crypto_quant {

// 单例实例（使用静态局部变量，C++11保证线程安全）
// 使用静态变量存储单例实例，确保全局唯一
static std::shared_ptr<IStrategyEngine> g_strategy_engine_instance = nullptr;
static std::shared_ptr<IOrderExecutor> g_order_executor_instance = nullptr;
static std::shared_ptr<IOrderbookManager> g_orderbook_manager_instance = nullptr;
static std::shared_ptr<IMarketDataProvider> g_market_data_provider_instance = nullptr;

// 互斥锁保护单例创建（双重检查锁定模式）
// 每个组件有独立的互斥锁，避免不必要的锁竞争
static std::mutex g_strategy_engine_mutex;
static std::mutex g_order_executor_mutex;
static std::mutex g_orderbook_manager_mutex;
static std::mutex g_market_data_provider_mutex;

// 工厂类实现 - 单例模式
// 使用双重检查锁定模式（Double-Checked Locking）实现线程安全的单例
std::shared_ptr<IStrategyEngine> CryptoQuantFactory::createStrategyEngine() {
    // 第一次检查（无锁，快速路径）
    if (g_strategy_engine_instance == nullptr) {
        // 加锁
        std::lock_guard<std::mutex> lock(g_strategy_engine_mutex);
        // 第二次检查（防止多线程同时创建）
        if (g_strategy_engine_instance == nullptr) {
            g_strategy_engine_instance = std::shared_ptr<IStrategyEngine>(new StrategyEngine());
        }
    }
    return g_strategy_engine_instance;
}

std::shared_ptr<IOrderExecutor> CryptoQuantFactory::createOrderExecutor() {
    if (g_order_executor_instance == nullptr) {
        std::lock_guard<std::mutex> lock(g_order_executor_mutex);
        if (g_order_executor_instance == nullptr) {
            g_order_executor_instance = std::shared_ptr<IOrderExecutor>(new OrderExecutor());
        }
    }
    return g_order_executor_instance;
}

std::shared_ptr<IOrderbookManager> CryptoQuantFactory::createOrderbookManager() {
    if (g_orderbook_manager_instance == nullptr) {
        std::lock_guard<std::mutex> lock(g_orderbook_manager_mutex);
        if (g_orderbook_manager_instance == nullptr) {
            g_orderbook_manager_instance = std::shared_ptr<IOrderbookManager>(new OrderbookManager());
        }
    }
    return g_orderbook_manager_instance;
}

std::shared_ptr<IMarketDataProvider> CryptoQuantFactory::createMarketDataProvider() {
    if (g_market_data_provider_instance == nullptr) {
        std::lock_guard<std::mutex> lock(g_market_data_provider_mutex);
        if (g_market_data_provider_instance == nullptr) {
            g_market_data_provider_instance = std::shared_ptr<IMarketDataProvider>(new MarketDataProvider());
        }
    }
    return g_market_data_provider_instance;
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
