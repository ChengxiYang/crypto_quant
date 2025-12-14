#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>
#include <spdlog/spdlog.h>

#include "market_data_fetcher.h"

namespace crypto_quant
{

    MarketDataFetcher::MarketDataFetcher()
        : is_running(false),
          use_binance(true), use_coingecko(true),
          rest_client_ptr(nullptr), websocket_client_ptr(nullptr),
          retry_count(0)
    {
        spdlog::debug("MarketDataFetcher constructor called");
    }

    MarketDataFetcher::~MarketDataFetcher()
    {
        if (is_running.load())
        {
            stop();
        }
        cleanupClients();
        spdlog::debug("MarketDataFetcher destructor called");
    }

    bool MarketDataFetcher::initialize()
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        // 根据数据源选择初始化相应的客户端
        bool success = true;
        
        if (use_binance.load()) {
            if (!initializeRESTClient()) {
                spdlog::warn("Failed to initialize REST client, will use fallback mode");
                success = false;
            }
        }
 
        spdlog::info("MarketDataFetcher initialized (REST: {}, WebSocket: {})", 
                     rest_client_ptr != nullptr, websocket_client_ptr != nullptr);
        return success;
    }

    int MarketDataFetcher::start(symbol_t symbol)
    {
        if (is_running.load())
        {
            spdlog::warn("Market data fetcher already running");
            return 0;
        }

        std::lock_guard<std::mutex> lock(mutex);
        is_running.store(true);
        current_symbol = symbol;

        // 如果使用 WebSocket，优先使用 WebSocket 实时数据
        if (use_binance.load() && websocket_client_ptr == nullptr) {
            initializeWebSocketClient(symbol);
        }
        
        // 启动数据收集线程（用于 REST 轮询或备用模式）
        data_thread = std::thread([this, symbol]()
                                  {
            int consecutive_errors = 0;
            const int MAX_CONSECUTIVE_ERRORS = 5;
            
            while (is_running.load()) {
                try {
                    // 获取当前交易对（线程安全）
                    symbol_t current_sym;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        current_sym = current_symbol;
                    }
                    
                    orderbook_t orderbook;
                    bool data_fetched = false;
                    
                    // 优先从 REST API 获取数据
                    if (use_binance.load() && rest_client_ptr != nullptr) {
                        data_fetched = fetchOrderbookFromREST(current_sym, orderbook);
                    }
                    
                    // 如果 REST 获取失败，使用模拟数据作为备用
                    if (!data_fetched) {
                        orderbook = generateOrderbook(current_sym);
                        if (consecutive_errors < MAX_CONSECUTIVE_ERRORS) {
                            consecutive_errors++;
                            if (consecutive_errors == MAX_CONSECUTIVE_ERRORS) {
                                spdlog::warn("Using fallback mode after {} consecutive errors", 
                                           MAX_CONSECUTIVE_ERRORS);
                            }
                        }
                    } else {
                        consecutive_errors = 0;
                    }
                    
                    // 调用回调函数（如果已设置）
                    std::function<void(const orderbook_t&)> callback;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        callback = orderbook_callback;
                    }
                    
                    if (callback) {
                        callback(orderbook);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error in market data thread: {}", e.what());
                }
                
                // 如果使用 WebSocket，减少 REST 轮询频率
                int sleep_ms = (websocket_client_ptr != nullptr) ? 5000 : 1000;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            } });

        spdlog::info("Market data fetcher started for symbol: {}", static_cast<int>(symbol));
        return 0;
    }

    void MarketDataFetcher::stop()
    {
        if (!is_running.load())
        {
            spdlog::warn("Market data fetcher already stopped");
            return;
        }

        is_running.store(false);
        
        // 停止 WebSocket 客户端
        if (websocket_client_ptr != nullptr) {
            websocket_client_stop(websocket_client_ptr);
        }
        
        // 等待线程结束
        if (data_thread.joinable())
        {
            data_thread.join();
        }
        
        spdlog::info("Market data fetcher stopped");
    }

    void MarketDataFetcher::setOrderbookCallback(std::function<void(const orderbook_t &)> callback)
    {
        std::lock_guard<std::mutex> lock(mutex);
        orderbook_callback = callback;
        spdlog::debug("C++ callback set");
    }

    orderbook_t MarketDataFetcher::getOrderbook(symbol_t symbol) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        orderbook_t orderbook;
        // 尝试从 REST API 获取
        if (use_binance.load() && rest_client_ptr != nullptr) {
            if (fetchOrderbookFromREST(symbol, orderbook)) {
                return orderbook;
            }
        }
        
        // 如果失败，返回模拟数据
        return generateOrderbook(symbol);
    }

    void MarketDataFetcher::setApiKey(const std::string &api_key, const std::string &api_secret)
    {
        std::lock_guard<std::mutex> lock(mutex);
        this->api_key = api_key;
        this->api_secret = api_secret;
        
        // 如果 REST 客户端已初始化，更新凭据
        if (rest_client_ptr != nullptr) {
            rest_client_set_credentials(rest_client_ptr, api_key.c_str(), api_secret.c_str());
        }
        
        spdlog::info("API credentials set");
    }

    void MarketDataFetcher::setDataSources(bool use_binance, bool use_coingecko)
    {
        this->use_binance.store(use_binance);
        this->use_coingecko.store(use_coingecko);
        spdlog::info("Data sources set: binance={}, coingecko={}", use_binance, use_coingecko);
    }

    orderbook_t MarketDataFetcher::generateOrderbook(symbol_t symbol) const
    {
        orderbook_t orderbook = {};
        orderbook.symbol = symbol;
        orderbook.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();

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
    
    bool MarketDataFetcher::fetchOrderbookFromREST(symbol_t symbol, orderbook_t& orderbook) const
    {
        if (!rest_client_ptr) {
            return false;
        }
        
        int result = rest_client_get_orderbook(rest_client_ptr, symbol, &orderbook);
        if (result == 0) {
            spdlog::debug("Successfully fetched orderbook from REST API for symbol: {}", 
                         static_cast<int>(symbol));
            return true;
        } else {
            spdlog::debug("Failed to fetch orderbook from REST API for symbol: {}", 
                         static_cast<int>(symbol));
            return false;
        }
    }
    
    bool MarketDataFetcher::initializeRESTClient()
    {
        if (rest_client_ptr != nullptr) {
            return true; // 已经初始化
        }
        
        const char* binance_base_url = "https://api.binance.com";
        rest_client_ptr = rest_client_create(binance_base_url);
        
        if (rest_client_ptr == nullptr) {
            spdlog::error("Failed to create REST client");
            return false;
        }
        
        // 设置 API 凭据（如果已提供）
        if (!api_key.empty() && !api_secret.empty()) {
            rest_client_set_credentials(rest_client_ptr, api_key.c_str(), api_secret.c_str());
        }
        
        spdlog::info("REST client initialized");
        return true;
    }
    
    bool MarketDataFetcher::initializeWebSocketClient(symbol_t symbol)
    {
        if (websocket_client_ptr != nullptr) {
            return true; // 已经初始化
        }
        
        // 构建 WebSocket URL（币安深度流）
        std::string binance_symbol = symbolToBinanceSymbol(symbol);
        std::transform(binance_symbol.begin(), binance_symbol.end(), 
                      binance_symbol.begin(), ::tolower);
        std::string ws_url = "wss://stream.binance.com:9443/ws/" + binance_symbol + "@depth20@100ms";
        
        websocket_client_ptr = websocket_client_create(ws_url.c_str());
        
        if (websocket_client_ptr == nullptr) {
            spdlog::error("Failed to create WebSocket client");
            return false;
        }
        
        // 设置回调函数
        websocket_client_set_callback(websocket_client_ptr, 
                                     websocketCallbackWrapper, 
                                     this);
        
        // 启动 WebSocket 连接
        int result = websocket_client_start(websocket_client_ptr);
        if (result != 0) {
            spdlog::error("Failed to start WebSocket client");
            websocket_client_destroy(websocket_client_ptr);
            websocket_client_ptr = nullptr;
            return false;
        }
        
        spdlog::info("WebSocket client initialized for symbol: {}", binance_symbol);
        return true;
    }
    
    void MarketDataFetcher::cleanupClients()
    {
        if (websocket_client_ptr != nullptr) {
            websocket_client_destroy(websocket_client_ptr);
            websocket_client_ptr = nullptr;
        }
        
        if (rest_client_ptr != nullptr) {
            rest_client_destroy(rest_client_ptr);
            rest_client_ptr = nullptr;
        }
    }
    
    void MarketDataFetcher::websocketCallbackWrapper(const orderbook_t* orderbook, void* user_data)
    {
        if (!orderbook || !user_data) {
            return;
        }
        
        MarketDataFetcher* fetcher = static_cast<MarketDataFetcher*>(user_data);
        
        // 获取回调函数并调用
        std::function<void(const orderbook_t&)> callback;
        {
            std::lock_guard<std::mutex> lock(fetcher->mutex);
            callback = fetcher->orderbook_callback;
        }
        
        if (callback) {
            callback(*orderbook);
        }
    }
    
    std::string MarketDataFetcher::symbolToBinanceSymbol(symbol_t symbol)
    {
        switch (symbol) {
            case SYMBOL_BTC_USDT:
                return "BTCUSDT";
            case SYMBOL_ETH_USDT:
                return "ETHUSDT";
            case SYMBOL_BTC_ETH:
                return "BTCETH";
            default:
                return "BTCUSDT";
        }
    }

} // namespace crypto_quant