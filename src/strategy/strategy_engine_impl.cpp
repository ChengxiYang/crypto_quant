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

// 订单执行器实现
class OrderExecutor : public IOrderExecutor {
private:
    std::atomic<bool> initialized_;
    std::atomic<ExecutionStatus> status_;
    RiskParams risk_params_;
    std::string api_key_;
    std::string api_secret_;
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, ExecutionResult> orders_;

public:
    OrderExecutor() : initialized_(false), status_(ExecutionStatus::IDLE) {}

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_.store(true);
        spdlog::info("OrderExecutor initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_.load() == ExecutionStatus::CONNECTED) {
            disconnect();
        }
        initialized_.store(false);
        spdlog::info("OrderExecutor cleaned up");
    }

    void setRiskParams(const RiskParams& params) override {
        std::lock_guard<std::mutex> lock(mutex_);
        risk_params_ = params;
        spdlog::info("Risk parameters set in OrderExecutor");
    }

    void setApiCredentials(const std::string& api_key, const std::string& api_secret) override {
        std::lock_guard<std::mutex> lock(mutex_);
        api_key_ = api_key;
        api_secret_ = api_secret;
        spdlog::info("API credentials set in OrderExecutor");
    }

    bool connect() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.store(ExecutionStatus::CONNECTED);
        spdlog::info("OrderExecutor connected");
        return true;
    }

    void disconnect() override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.store(ExecutionStatus::DISCONNECTED);
        spdlog::info("OrderExecutor disconnected");
    }

    ExecutionStatus getStatus() const override {
        return status_.load();
    }

    ExecutionResult submitOrder(symbol_t symbol, int side, double price, double quantity) override {
        std::lock_guard<std::mutex> lock(mutex_);
        ExecutionResult result;
        result.order_id = static_cast<uint64_t>(std::time(nullptr)) * 1000 + orders_.size();
        result.status = ExecutionResultStatus::SUCCESS;
        result.filled_quantity = quantity;
        result.average_price = price;
        
        orders_[result.order_id] = result;
        spdlog::info("Order submitted: id={}, symbol={}, side={}, price={:.2f}, quantity={:.2f}", 
                    result.order_id, static_cast<int>(symbol), side, price, quantity);
        return result;
    }

    bool cancelOrder(uint64_t order_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = orders_.find(order_id);
        if (it != orders_.end()) {
            orders_.erase(it);
            spdlog::info("Order cancelled: id={}", order_id);
            return true;
        }
        spdlog::warn("Order not found for cancellation: id={}", order_id);
        return false;
    }

    double getBalance(symbol_t symbol) override {
        // 模拟余额
        return 10000.0;
    }

    double getPosition(symbol_t symbol) override {
        // 模拟持仓
        return 0.0;
    }

    ExecutionResult getOrderStatus(uint64_t order_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = orders_.find(order_id);
        if (it != orders_.end()) {
            return it->second;
        }
        ExecutionResult result;
        result.status = ExecutionResultStatus::FAILED;
        result.error_message = "Order not found";
        return result;
    }

    std::vector<uint64_t> getOrderHistory(int max_count) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint64_t> order_ids;
        for (const auto& pair : orders_) {
            if (order_ids.size() >= static_cast<size_t>(max_count)) break;
            order_ids.push_back(pair.first);
        }
        return order_ids;
    }
};

// 订单薄管理器实现
class OrderbookManager : public IOrderbookManager {
private:
    std::atomic<bool> initialized_;
    mutable std::mutex mutex_;
    std::unordered_map<symbol_t, orderbook_t> orderbooks_;

public:
    OrderbookManager() : initialized_(false) {}

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_.store(true);
        spdlog::info("OrderbookManager initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        orderbooks_.clear();
        initialized_.store(false);
        spdlog::info("OrderbookManager cleaned up");
    }

    void updateOrderbook(const orderbook_t& orderbook) override {
        std::lock_guard<std::mutex> lock(mutex_);
        orderbooks_[orderbook.symbol] = orderbook;
        spdlog::debug("Orderbook updated for symbol: {}", static_cast<int>(orderbook.symbol));
    }

    orderbook_t getOrderbook(symbol_t symbol) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = orderbooks_.find(symbol);
        if (it != orderbooks_.end()) {
            return it->second;
        }
        return orderbook_t{};
    }

    double getBestBid(symbol_t symbol) const override {
        orderbook_t orderbook = getOrderbook(symbol);
        if (orderbook.bid_count > 0) {
            return orderbook.bids[0].price;
        }
        return 0.0;
    }

    double getBestAsk(symbol_t symbol) const override {
        orderbook_t orderbook = getOrderbook(symbol);
        if (orderbook.ask_count > 0) {
            return orderbook.asks[0].price;
        }
        return 0.0;
    }

    double getMidPrice(symbol_t symbol) const override {
        double bid = getBestBid(symbol);
        double ask = getBestAsk(symbol);
        if (bid > 0 && ask > 0) {
            return (bid + ask) / 2.0;
        }
        return 0.0;
    }

    double getSpread(symbol_t symbol) const override {
        double bid = getBestBid(symbol);
        double ask = getBestAsk(symbol);
        if (bid > 0 && ask > 0) {
            return ask - bid;
        }
        return 0.0;
    }

    double getBidDepth(symbol_t symbol, int levels) const override {
        orderbook_t orderbook = getOrderbook(symbol);
        double depth = 0.0;
        int count = std::min(levels, static_cast<int>(orderbook.bid_count));
        for (int i = 0; i < count; i++) {
            depth += orderbook.bids[i].quantity;
        }
        return depth;
    }

    double getAskDepth(symbol_t symbol, int levels) const override {
        orderbook_t orderbook = getOrderbook(symbol);
        double depth = 0.0;
        int count = std::min(levels, static_cast<int>(orderbook.ask_count));
        for (int i = 0; i < count; i++) {
            depth += orderbook.asks[i].quantity;
        }
        return depth;
    }

    uint64_t getTimestamp(symbol_t symbol) const override {
        orderbook_t orderbook = getOrderbook(symbol);
        return orderbook.timestamp;
    }

    bool isValid(symbol_t symbol) const override {
        orderbook_t orderbook = getOrderbook(symbol);
        return orderbook.bid_count > 0 && orderbook.ask_count > 0;
    }
};

} // namespace crypto_quant

// MarketDataProvider实现
namespace crypto_quant {

class MarketDataProvider : public IMarketDataProvider {
private:
    std::function<void(const orderbook_t&)> callback_; // 市场数据回调函数，todo 改成函数指针
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    std::string api_key_;
    std::string api_secret_;
    bool use_binance_;
    bool use_coingecko_;
    mutable std::mutex mutex_;
    std::thread data_thread_;
    symbol_t current_symbol_;

public:
    MarketDataProvider() : initialized_(false), running_(false), 
                          use_binance_(true), use_coingecko_(true) {}

    bool initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_.store(true);
        spdlog::info("MarketDataProvider initialized");
        return true;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_.load()) {
            stop();
        }
        initialized_.store(false);
        spdlog::info("MarketDataProvider cleaned up");
    }

    void setCallback(std::function<void(const orderbook_t&)> callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = callback;
        spdlog::info("Market data callback set");
    }

    bool start(symbol_t symbol) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!initialized_.load()) {
            spdlog::error("MarketDataProvider not initialized");
            return false;
        }
        
        if (running_.load()) {
            spdlog::warn("Market data collection already running");
            return true;
        }
        
        running_.store(true);
        current_symbol_ = symbol;
        
        // 启动数据收集线程
        data_thread_ = std::thread([this]() {
            while (running_.load()) {
                // 从币安获取真实订单薄数据
                orderbook_t orderbook = generateOrderbook(current_symbol_);
                
                // 调用回调函数
                if (callback_) {
                    callback_(orderbook);
                }
                
                // 每秒更新一次（币安API限制：每分钟1200次请求）
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        });
        
        spdlog::info("Market data collection started for symbol: {}", static_cast<int>(symbol));
        return true;
    }

    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.load()) {
            spdlog::warn("Market data collection already stopped");
            return;
        }
        
        running_.store(false);
        if (data_thread_.joinable()) {
            data_thread_.join();
        }
        spdlog::info("Market data collection stopped");
    }

    orderbook_t getOrderbook(symbol_t symbol) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return generateOrderbook(symbol);
    }

    void setApiKey(const std::string& api_key, const std::string& api_secret) override {
        std::lock_guard<std::mutex> lock(mutex_);
        api_key_ = api_key;
        api_secret_ = api_secret;
        spdlog::info("API key set for MarketDataProvider");
    }

    void setDataSources(bool use_binance, bool use_coingecko) override {
        std::lock_guard<std::mutex> lock(mutex_);
        use_binance_ = use_binance;
        use_coingecko_ = use_coingecko;
        spdlog::info("Data sources set: binance={}, coingecko={}", use_binance, use_coingecko);
    }

private:
    // 从币安API获取真实订单薄数据
    orderbook_t fetchOrderbookFromBinance(symbol_t symbol) const {
        orderbook_t orderbook = {};
        memset(&orderbook, 0, sizeof(orderbook_t));
        orderbook.symbol = symbol;
        
        // 转换交易对符号
        std::string binance_symbol;
        switch (symbol) {
            case SYMBOL_BTC_USDT: binance_symbol = "BTCUSDT"; break;
            case SYMBOL_ETH_USDT: binance_symbol = "ETHUSDT"; break;
            case SYMBOL_BTC_ETH: binance_symbol = "BTCETH"; break;
            default: binance_symbol = "BTCUSDT"; break;
        }
        
        // 使用REST客户端获取订单薄
        struct rest_client* client = rest_client_create("https://api.binance.com");
        if (!client) {
            spdlog::error("Failed to create REST client for market data");
            return orderbook;
        }
        
        // 获取订单薄数据
        int result = rest_client_get_orderbook(client, symbol, &orderbook);
        if (result != 0) {
            spdlog::warn("Failed to fetch orderbook from Binance, using fallback");
            // 如果失败，使用简单的价格查询作为后备
            orderbook = generateFallbackOrderbook(symbol);
        }
        
        rest_client_destroy(client);
        return orderbook;
    }
    
    // 后备方案：生成简单订单薄（当API失败时）
    orderbook_t generateFallbackOrderbook(symbol_t symbol) const {
        orderbook_t orderbook = {};
        orderbook.symbol = symbol;
        orderbook.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // 根据交易对生成不同的价格
        double base_price = 50000.0 + (static_cast<int>(symbol) * 1000.0);
        orderbook.bid_count = 1;
        orderbook.ask_count = 1;
        orderbook.bids[0].price = base_price - 5.0;
        orderbook.bids[0].quantity = 1.0;
        orderbook.asks[0].price = base_price + 5.0;
        orderbook.asks[0].quantity = 1.0;
        
        return orderbook;
    }
    
    orderbook_t generateOrderbook(symbol_t symbol) const {
        // 如果启用了币安，尝试从币安获取真实数据
        if (use_binance_) {
            return fetchOrderbookFromBinance(symbol);
        }
        // 否则使用后备方案
        return generateFallbackOrderbook(symbol);
    }
};

} // namespace crypto_quant
