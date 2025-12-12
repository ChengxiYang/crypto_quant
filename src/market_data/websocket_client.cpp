#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "market_data.h"

using json = nlohmann::json;

// C++风格的WebSocket客户端结构
struct websocket_client {
    std::string url;
    CURL* curl;
    std::thread worker_thread;
    std::atomic<bool> is_running;
    market_data_callback_t callback;
    void* user_data;
    mutable std::mutex mutex;
    std::atomic<bool> initialized;
    
    websocket_client() : curl(nullptr), is_running(false), initialized(false) {}
    
    ~websocket_client() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
};

// WebSocket数据回调函数
static size_t websocket_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    websocket_client* client = static_cast<websocket_client*>(userdata);
    size_t realsize = size * nmemb;
    
    if (realsize > 0) {
        std::string data(ptr, realsize);
        
        try {
            // 解析JSON数据
            json j = json::parse(data);
            
            if (j.contains("stream") && j.contains("data")) {
                std::string stream_name = j["stream"].get<std::string>();
                
                if (stream_name.find("@depth") != std::string::npos) {
                    // 解析订单薄数据
                    orderbook_t orderbook;
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
                    
                    // 调用回调函数
                    if (client->callback) {
                        client->callback(&orderbook, client->user_data);
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
    }
    
    return realsize;
}

// WebSocket工作线程函数
static void websocket_thread_func(websocket_client* client) {
    if (!client || !client->initialized.load()) {
        return;
    }
    
    spdlog::info("WebSocket thread started for URL: {}", client->url);
    
    curl_easy_setopt(client->curl, CURLOPT_URL, client->url.c_str());
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, websocket_write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, client);
    
    // 设置WebSocket协议头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Connection: Upgrade");
    headers = curl_slist_append(headers, "Upgrade: websocket");
    headers = curl_slist_append(headers, "Sec-WebSocket-Version: 13");
    headers = curl_slist_append(headers, "Sec-WebSocket-Key: SGVsbG8sIFdvcmxkIQ=="); // 示例key
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res;
    while (client->is_running.load()) {
        res = curl_easy_perform(client->curl);
        if (res != CURLE_OK) {
            spdlog::error("WebSocket connection failed: {}", curl_easy_strerror(res));
            // 等待5秒后重试
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    curl_slist_free_all(headers);
    spdlog::info("WebSocket thread ended");
}

// 创建WebSocket客户端
struct websocket_client* websocket_client_create(const char* url) {
    try {
        auto* client = new websocket_client();
        
        if (url) {
            client->url = url;
        }
        
        client->curl = curl_easy_init();
        if (!client->curl) {
            delete client;
            spdlog::error("Failed to initialize CURL for WebSocket");
            return nullptr;
        }
        
        client->initialized.store(true);
        spdlog::debug("WebSocket client created for URL: {}", client->url);
        return client;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create WebSocket client: {}", e.what());
        return nullptr;
    }
}

// 前向声明
static int websocket_client_stop(struct websocket_client* client);

// 销毁WebSocket客户端
void websocket_client_destroy(struct websocket_client* client) {
    if (client) {
        websocket_client_stop(client);
        delete client;
        spdlog::debug("WebSocket client destroyed");
    }
}

// 设置回调函数
int websocket_client_set_callback(struct websocket_client* client, 
                                 market_data_callback_t callback, 
                                 void* user_data) {
    if (!client || !client->initialized.load()) {
        spdlog::error("Invalid WebSocket client");
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(client->mutex);
    client->callback = callback;
    client->user_data = user_data;
    spdlog::debug("WebSocket callback set");
    return 0;
}

// 启动WebSocket客户端
int websocket_client_start(struct websocket_client* client) {
    if (!client || !client->initialized.load()) {
        spdlog::error("Invalid WebSocket client");
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(client->mutex);
    if (client->is_running.load()) {
        spdlog::warn("WebSocket client already running");
        return 0; // 已经运行
    }
    
    client->is_running.store(true);
    
    try {
        client->worker_thread = std::thread(websocket_thread_func, client);
        spdlog::info("WebSocket client started");
        return 0;
    } catch (const std::exception& e) {
        client->is_running.store(false);
        spdlog::error("Failed to start WebSocket thread: {}", e.what());
        return -1;
    }
}

// 停止WebSocket客户端
int websocket_client_stop(struct websocket_client* client) {
    if (!client || !client->initialized.load()) {
        spdlog::error("Invalid WebSocket client");
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(client->mutex);
    if (!client->is_running.load()) {
        spdlog::warn("WebSocket client not running");
        return 0; // 已经停止
    }
    
    client->is_running.store(false);
    
    if (client->worker_thread.joinable()) {
        client->worker_thread.join();
    }
    
    spdlog::info("WebSocket client stopped");
    return 0;
}
