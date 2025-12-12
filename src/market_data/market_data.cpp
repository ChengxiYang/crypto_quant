#include <cstdlib>
#include <cstring>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <functional>
#include <spdlog/spdlog.h>
#include "crypto_quant.h"

// 统一的市场数据获取器实现
class MarketDataFetcher {
private:
    market_data_callback_t c_callback;
    void* c_user_data;
    std::function<void(const orderbook_t&)> cpp_callback;
    std::atomic<bool> is_running;
    std::string api_key;
    std::string api_secret;
    std::atomic<bool> use_binance;
    std::atomic<bool> use_coingecko;
    mutable std::mutex mutex;
    std::thread data_thread;
    symbol_t current_symbol;

public:
    MarketDataFetcher() : c_callback(nullptr), c_user_data(nullptr), is_running(false), use_binance(true), use_coingecko(true) {
        spdlog::debug("MarketDataFetcher constructor called");
    }

    ~MarketDataFetcher() {
        if (is_running.load()) {
            stop();
        }
        spdlog::debug("MarketDataFetcher destructor called");
    }

    // C风格接口
    void setCCallback(market_data_callback_t callback, void* user_data) {
        std::lock_guard<std::mutex> lock(mutex);
        c_callback = callback;
        c_user_data = user_data;
        spdlog::debug("C callback set");
    }

    int start(symbol_t symbol) {
        if (is_running.load()) {
            spdlog::warn("Market data fetcher already running");
            return 0;
        }

        std::lock_guard<std::mutex> lock(mutex);
        is_running.store(true);
        current_symbol = symbol;
        
        // 启动数据收集线程
        data_thread = std::thread([this]() {
            while (is_running.load()) {
                // 生成模拟数据
                orderbook_t orderbook = generateOrderbook(current_symbol);
                
                // 调用C回调
                if (c_callback) {
                    c_callback(&orderbook, c_user_data);
                }
                
                // 调用C++回调
                if (cpp_callback) {
                    cpp_callback(orderbook);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        });
        
        spdlog::info("Market data fetcher started for symbol: {}", static_cast<int>(symbol));
        return 0;
    }

    void stop() {
        if (!is_running.load()) {
            spdlog::warn("Market data fetcher already stopped");
            return;
        }
        
        is_running.store(false);
        if (data_thread.joinable()) {
            data_thread.join();
        }
        spdlog::info("Market data fetcher stopped");
    }

    int getOrderbook(symbol_t symbol, orderbook_t* orderbook) {
        if (!orderbook) {
            spdlog::error("Invalid orderbook pointer");
            return -1;
        }
        
        std::lock_guard<std::mutex> lock(mutex);
        *orderbook = generateOrderbook(symbol);
        return 0;
    }

    void setApiKey(const char* api_key, const char* api_secret) {
        std::lock_guard<std::mutex> lock(mutex);
        if (api_key) {
            this->api_key = api_key;
        }
        if (api_secret) {
            this->api_secret = api_secret;
        }
        spdlog::info("API credentials set");
    }

    void setDataSources(int use_binance, int use_coingecko) {
        this->use_binance.store(use_binance != 0);
        this->use_coingecko.store(use_coingecko != 0);
        spdlog::info("Data sources set: binance={}, coingecko={}", 
                    this->use_binance.load(), this->use_coingecko.load());
    }

    // C++风格接口
    void setCppCallback(std::function<void(const orderbook_t&)> callback) {
        std::lock_guard<std::mutex> lock(mutex);
        cpp_callback = callback;
        spdlog::debug("C++ callback set");
    }

    orderbook_t getOrderbook(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex);
        return generateOrderbook(symbol);
    }

    void setApiKey(const std::string& api_key, const std::string& api_secret) {
        std::lock_guard<std::mutex> lock(mutex);
        this->api_key = api_key;
        this->api_secret = api_secret;
        spdlog::info("API credentials set");
    }

    void setDataSources(bool use_binance, bool use_coingecko) {
        this->use_binance.store(use_binance);
        this->use_coingecko.store(use_coingecko);
        spdlog::info("Data sources set: binance={}, coingecko={}", use_binance, use_coingecko);
    }

private:
    orderbook_t generateOrderbook(symbol_t symbol) const {
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
        
        spdlog::debug("Orderbook data generated for symbol: {}, price: {:.2f}", 
                     static_cast<int>(symbol), base_price);
        return orderbook;
    }
};

// C风格API实现
extern "C" {

market_data_fetcher_t* market_data_fetcher_create(void) {
    try {
        auto* fetcher = new MarketDataFetcher();
        spdlog::debug("Market data fetcher created");
        return reinterpret_cast<market_data_fetcher_t*>(fetcher);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create market data fetcher: {}", e.what());
        return nullptr;
    }
}

void market_data_fetcher_destroy(market_data_fetcher_t* fetcher) {
    if (fetcher) {
        delete reinterpret_cast<MarketDataFetcher*>(fetcher);
        spdlog::debug("Market data fetcher destroyed");
    }
}

int market_data_fetcher_set_callback(market_data_fetcher_t* fetcher, 
                                   market_data_callback_t callback, 
                                   void* user_data) {
    if (!fetcher) {
        spdlog::error("Invalid fetcher pointer");
        return -1;
    }
    
    reinterpret_cast<MarketDataFetcher*>(fetcher)->setCCallback(callback, user_data);
    return 0;
}

int market_data_fetcher_start(market_data_fetcher_t* fetcher, symbol_t symbol) {
    if (!fetcher) {
        spdlog::error("Invalid fetcher pointer");
        return -1;
    }
    
    return reinterpret_cast<MarketDataFetcher*>(fetcher)->start(symbol);
}

int market_data_fetcher_stop(market_data_fetcher_t* fetcher) {
    if (!fetcher) {
        spdlog::error("Invalid fetcher pointer");
        return -1;
    }
    
    reinterpret_cast<MarketDataFetcher*>(fetcher)->stop();
    return 0;
}

int market_data_fetcher_get_orderbook(market_data_fetcher_t* fetcher, 
                                    symbol_t symbol, 
                                    orderbook_t* orderbook) {
    if (!fetcher) {
        spdlog::error("Invalid fetcher pointer");
        return -1;
    }
    
    return reinterpret_cast<MarketDataFetcher*>(fetcher)->getOrderbook(symbol, orderbook);
}

int market_data_fetcher_set_api_key(market_data_fetcher_t* fetcher, 
                                   const char* api_key, 
                                   const char* api_secret) {
    if (!fetcher) {
        spdlog::error("Invalid fetcher pointer");
        return -1;
    }
    
    reinterpret_cast<MarketDataFetcher*>(fetcher)->setApiKey(api_key, api_secret);
    return 0;
}

int market_data_fetcher_set_data_sources(market_data_fetcher_t* fetcher, 
                                        int use_binance, 
                                        int use_coingecko) {
    if (!fetcher) {
        spdlog::error("Invalid fetcher pointer");
        return -1;
    }
    
    reinterpret_cast<MarketDataFetcher*>(fetcher)->setDataSources(use_binance, use_coingecko);
    return 0;
}

} // extern "C"