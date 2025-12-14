#include <chrono>
#include <thread>
#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>

#include "market_data_fetcher.h"
#include "websocket_client.h"

namespace crypto_quant
{

    MarketDataFetcher::MarketDataFetcher()
        : is_running(false),
          use_binance(true), use_coingecko(true)
    {
        spdlog::debug("MarketDataFetcher constructor called");
    }

    MarketDataFetcher::~MarketDataFetcher()
    {
        if (is_running.load())
        {
            stop();
        }
        spdlog::debug("MarketDataFetcher destructor called");
    }

    bool MarketDataFetcher::initialize()
    {
        std::lock_guard<std::mutex> lock(mutex);
        spdlog::info("MarketDataFetcher initialized");
        return true;
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

        // 如果使用 WebSocket，初始化 WebSocket 客户端
        if (use_binance.load()) {
            initializeWebSocketClient(symbol);
        }
        
        // 启动数据收集线程（用于备用模式）
        data_thread = std::thread([this]()
                                  {
            while (is_running.load()) {
                try {
                    // 获取当前交易对（线程安全）
                    symbol_t current_sym;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        current_sym = current_symbol;
                    }
                    
                    // 如果 WebSocket 未运行，使用模拟数据
                    bool use_fallback = true;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (websocket_client_ && websocket_client_->isRunning()) {
                            use_fallback = false;
                        }
                    }
                    
                    if (use_fallback) {
                        orderbook_t orderbook = generateOrderbook(current_sym);
                        
                        // 调用回调函数（如果已设置）
                        std::function<void(const orderbook_t&)> callback;
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            callback = orderbook_callback;
                        }
                        
                        if (callback) {
                            callback(orderbook);
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error in market data thread: {}", e.what());
                }
                
                // 如果使用 WebSocket，减少轮询频率
                int sleep_ms = (websocket_client_ && websocket_client_->isRunning()) ? 5000 : 1000;
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
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (websocket_client_) {
                websocket_client_->stop();
                websocket_client_.reset();
            }
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
        spdlog::debug("Orderbook callback set");
    }

    orderbook_t MarketDataFetcher::getOrderbook(symbol_t symbol) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        // 返回模拟数据
        return generateOrderbook(symbol);
    }

    void MarketDataFetcher::setApiKey(const std::string &api_key, const std::string &api_secret)
    {
        std::lock_guard<std::mutex> lock(mutex);
        this->api_key = api_key;
        this->api_secret = api_secret;
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
    
    bool MarketDataFetcher::initializeWebSocketClient(symbol_t symbol)
    {
        if (websocket_client_) {
            return true; // 已经初始化
        }
        
        // 构建 WebSocket URL（币安深度流）
        std::string binance_symbol = symbolToBinanceSymbol(symbol);
        std::transform(binance_symbol.begin(), binance_symbol.end(), 
                      binance_symbol.begin(), ::tolower);
        std::string ws_url = "wss://stream.binance.com:9443/ws/" + binance_symbol + "@depth20@100ms";
        
        try {
            websocket_client_ = std::unique_ptr<WebSocketClient>(new WebSocketClient(ws_url));
            
            if (!websocket_client_->isInitialized()) {
                spdlog::error("Failed to initialize WebSocket client");
                websocket_client_.reset();
                return false;
            }
            
            // 设置回调函数
            websocket_client_->setCallback([this](const orderbook_t* orderbook) {
                if (!orderbook) {
                    return;
                }
                
                // 调用回调函数（如果已设置）
                std::function<void(const orderbook_t&)> callback;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    callback = orderbook_callback;
                }
                
                if (callback) {
                    callback(*orderbook);
                }
            });
            
            // 启动 WebSocket 连接
            if (!websocket_client_->start()) {
                spdlog::error("Failed to start WebSocket client");
                websocket_client_.reset();
                return false;
            }
            
            spdlog::info("WebSocket client initialized for symbol: {}", binance_symbol);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Exception creating WebSocket client: {}", e.what());
            websocket_client_.reset();
            return false;
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
