#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "websocket_client.h"

using json = nlohmann::json;

namespace crypto_quant
{

// CURL 回调函数
size_t WebSocketClient::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    WebSocketClient* client = static_cast<WebSocketClient*>(userdata);
    return client->onDataReceived(ptr, size * nmemb);
}

// 处理接收到的数据
size_t WebSocketClient::onDataReceived(char* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    std::string data_str(data, size);
    
    try {
        json j = json::parse(data_str);
        
        if (j.contains("stream") && j.contains("data")) {
            std::string stream_name = j["stream"].get<std::string>();
            
            if (stream_name.find("@depth") != std::string::npos) {
                orderbook_t orderbook = parseOrderbook(&j, stream_name);
                
                // 调用回调函数
                std::function<void(const orderbook_t*)> callback;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    callback = callback_;
                }
                
                if (callback) {
                    callback(&orderbook);
                }
                
                spdlog::debug("WebSocket orderbook data processed: {} bids, {} asks", 
                             orderbook.bid_count, orderbook.ask_count);
            }
        }
    } catch (const json::parse_error& e) {
        spdlog::error("JSON parse error in WebSocket: {}", e.what());
    } catch (const json::type_error& e) {
        spdlog::error("JSON type error in WebSocket: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error processing WebSocket data: {}", e.what());
    }
    
    return size;
}

// 解析订单薄数据（使用 void* 避免在头文件中暴露 json 类型）
orderbook_t WebSocketClient::parseOrderbook(const void* json_obj, const std::string& stream_name) const {
    const json& j = *static_cast<const json*>(json_obj);
    orderbook_t orderbook = {};
    memset(&orderbook, 0, sizeof(orderbook_t));
    
    // 确定交易对
    if (stream_name.find("btcusdt") != std::string::npos) {
        orderbook.symbol = SYMBOL_BTC_USDT;
    } else if (stream_name.find("ethusdt") != std::string::npos) {
        orderbook.symbol = SYMBOL_ETH_USDT;
    } else if (stream_name.find("btceth") != std::string::npos) {
        orderbook.symbol = SYMBOL_BTC_ETH;
    }
    
    orderbook.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    json data_obj = j["data"];
    
    // 解析买盘
    if (data_obj.contains("bids") && data_obj["bids"].is_array()) {
        uint32_t bid_count = static_cast<uint32_t>(data_obj["bids"].size());
        orderbook.bid_count = (bid_count < 20) ? bid_count : 20;
        
        for (uint32_t i = 0; i < orderbook.bid_count; i++) {
            if (data_obj["bids"][i].is_array() && data_obj["bids"][i].size() >= 2) {
                orderbook.bids[i].price = std::stod(data_obj["bids"][i][0].get<std::string>());
                orderbook.bids[i].quantity = std::stod(data_obj["bids"][i][1].get<std::string>());
            }
        }
    }
    
    // 解析卖盘
    if (data_obj.contains("asks") && data_obj["asks"].is_array()) {
        uint32_t ask_count = static_cast<uint32_t>(data_obj["asks"].size());
        orderbook.ask_count = (ask_count < 20) ? ask_count : 20;
        
        for (uint32_t i = 0; i < orderbook.ask_count; i++) {
            if (data_obj["asks"][i].is_array() && data_obj["asks"][i].size() >= 2) {
                orderbook.asks[i].price = std::stod(data_obj["asks"][i][0].get<std::string>());
                orderbook.asks[i].quantity = std::stod(data_obj["asks"][i][1].get<std::string>());
            }
        }
    }
    
    return orderbook;
}

// 工作线程函数
void WebSocketClient::workerThread() {
    if (!initialized_.load()) {
        return;
    }
    
    spdlog::info("WebSocket thread started for URL: {}", url_);
    
    curl_easy_setopt(static_cast<CURL*>(curl_), CURLOPT_URL, url_.c_str());
    curl_easy_setopt(static_cast<CURL*>(curl_), CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(static_cast<CURL*>(curl_), CURLOPT_WRITEDATA, this);
    
    // 设置 WebSocket 协议头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Connection: Upgrade");
    headers = curl_slist_append(headers, "Upgrade: websocket");
    headers = curl_slist_append(headers, "Sec-WebSocket-Version: 13");
    headers = curl_slist_append(headers, "Sec-WebSocket-Key: SGVsbG8sIFdvcmxkIQ==");
    curl_easy_setopt(static_cast<CURL*>(curl_), CURLOPT_HTTPHEADER, headers);
    
    CURLcode res;
    while (is_running_.load()) {
        res = curl_easy_perform(static_cast<CURL*>(curl_));
        if (res != CURLE_OK) {
            spdlog::error("WebSocket connection failed: {}", curl_easy_strerror(res));
            // 等待5秒后重试
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    curl_slist_free_all(headers);
    spdlog::info("WebSocket thread ended");
}

// 构造函数
WebSocketClient::WebSocketClient(const std::string& url) 
    : url_(url), curl_(nullptr), is_running_(false), initialized_(false) {
    curl_ = curl_easy_init();
    if (curl_) {
        initialized_.store(true);
        spdlog::debug("WebSocket client created for URL: {}", url_);
    } else {
        spdlog::error("Failed to initialize CURL for WebSocket");
    }
}

// 析构函数
WebSocketClient::~WebSocketClient() {
    stop();
    if (curl_) {
        curl_easy_cleanup(static_cast<CURL*>(curl_));
    }
}

// 设置回调函数
void WebSocketClient::setCallback(std::function<void(const orderbook_t*)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
    spdlog::debug("WebSocket callback set");
}

// 启动 WebSocket 连接
bool WebSocketClient::start() {
    if (!initialized_.load()) {
        spdlog::error("WebSocket client not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_.load()) {
        spdlog::warn("WebSocket client already running");
        return true;
    }
    
    is_running_.store(true);
    
    try {
        worker_thread_ = std::thread(&WebSocketClient::workerThread, this);
        spdlog::info("WebSocket client started");
        return true;
    } catch (const std::exception& e) {
        is_running_.store(false);
        spdlog::error("Failed to start WebSocket thread: {}", e.what());
        return false;
    }
}

// 停止 WebSocket 连接
bool WebSocketClient::stop() {
    if (!initialized_.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_running_.load()) {
        return true;
    }
    
    is_running_.store(false);
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    spdlog::info("WebSocket client stopped");
    return true;
}

bool WebSocketClient::isRunning() const {
    return is_running_.load();
}

bool WebSocketClient::isInitialized() const {
    return initialized_.load();
}

} // namespace crypto_quant
