#ifndef MARKET_DATA_FETCHER_H
#define MARKET_DATA_FETCHER_H

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

#include "crypto_quant.h"

// 前向声明（避免循环依赖）
struct rest_client;
struct websocket_client;

// 前向声明 C 函数
extern "C" {
    struct rest_client* rest_client_create(const char* base_url);
    void rest_client_destroy(struct rest_client* client);
    int rest_client_set_credentials(struct rest_client* client, const char* api_key, const char* api_secret);
    int rest_client_get_orderbook(struct rest_client* client, crypto_quant::symbol_t symbol, crypto_quant::orderbook_t* orderbook);
    
    struct websocket_client* websocket_client_create(const char* url);
    void websocket_client_destroy(struct websocket_client* client);
    typedef void (*market_data_callback_t)(const crypto_quant::orderbook_t*, void*);
    int websocket_client_set_callback(struct websocket_client* client, 
                                     market_data_callback_t callback, 
                                     void* user_data);
    int websocket_client_start(struct websocket_client* client);
    int websocket_client_stop(struct websocket_client* client);
}

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
    
    // REST 和 WebSocket 客户端
    struct rest_client* rest_client_ptr;
    struct websocket_client* websocket_client_ptr;
    
    // 重试和错误处理
    std::atomic<int> retry_count;
    static constexpr int MAX_RETRY_COUNT = 3;
    static constexpr int RETRY_DELAY_MS = 1000;

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
    
    // 从 REST API 获取订单薄数据
    bool fetchOrderbookFromREST(symbol_t symbol, orderbook_t& orderbook) const;
    
    // 初始化 REST 客户端
    bool initializeRESTClient();
    
    // 初始化 WebSocket 客户端
    bool initializeWebSocketClient(symbol_t symbol);
    
    // 清理客户端资源
    void cleanupClients();
    
    // WebSocket 回调包装器
    static void websocketCallbackWrapper(const orderbook_t* orderbook, void* user_data);
    
    // 将 symbol_t 转换为币安交易对字符串
    static std::string symbolToBinanceSymbol(symbol_t symbol);
};

} // namespace crypto_quant

#endif // MARKET_DATA_FETCHER_H
