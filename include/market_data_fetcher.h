#ifndef MARKET_DATA_FETCHER_H
#define MARKET_DATA_FETCHER_H

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <memory>

#include "crypto_quant.h"
#include "websocket_client.h"

namespace crypto_quant {

// 统一的市场数据获取器实现类
class MarketDataFetcher : public IMarketDataFetcher {
private:
    std::function<void(const orderbook_t&)> orderbook_callback;
    std::atomic<bool> is_running;
    std::string api_key;
    std::string api_secret;
    std::atomic<bool> use_binance;
    std::atomic<bool> use_coingecko;
    mutable std::mutex mutex;
    std::thread data_thread;
    symbol_t current_symbol;
    
    // WebSocket 客户端
    std::unique_ptr<WebSocketClient> websocket_client_;

public:
    MarketDataFetcher();
    ~MarketDataFetcher();
    bool initialize() override;
    int start(symbol_t symbol) override;
    void stop() override;
    void setApiKey(const std::string& api_key, const std::string& api_secret) override;
    void setDataSources(bool use_binance, bool use_coingecko) override;
    void setOrderbookCallback(std::function<void(const orderbook_t&)> callback) override;
    orderbook_t getOrderbook(symbol_t symbol) const override;

private:
    // 生成模拟订单薄数据（备用方案）
    orderbook_t generateOrderbook(symbol_t symbol) const;
    
    // 初始化 WebSocket 客户端
    bool initializeWebSocketClient(symbol_t symbol);
    
    // 将 symbol_t 转换为币安交易对字符串
    static std::string symbolToBinanceSymbol(symbol_t symbol);
};

} // namespace crypto_quant

#endif // MARKET_DATA_FETCHER_H
