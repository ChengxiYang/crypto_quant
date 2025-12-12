#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "market_data.h"

using json = nlohmann::json;

// C++风格的REST客户端结构
struct rest_client {
    std::string base_url;
    std::string api_key;
    std::string api_secret;
    CURL* curl;
    int timeout;
    std::atomic<bool> initialized;
    mutable std::mutex mutex;
    
    rest_client() : curl(nullptr), timeout(10), initialized(false) {}
    
    ~rest_client() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
};

// HTTP响应回调函数
static size_t rest_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), realsize);
    return realsize;
}

// 创建REST客户端
struct rest_client* rest_client_create(const char* base_url) {
    try {
        auto* client = new rest_client();
        
        if (base_url) {
            client->base_url = base_url;
        }
        
        client->curl = curl_easy_init();
        if (!client->curl) {
            delete client;
            spdlog::error("Failed to initialize CURL");
            return nullptr;
        }
        
        client->initialized.store(true);
        spdlog::debug("REST client created for URL: {}", client->base_url);
        return client;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create REST client: {}", e.what());
        return nullptr;
    }
}

// 销毁REST客户端
void rest_client_destroy(struct rest_client* client) {
    if (client) {
        delete client;
        spdlog::debug("REST client destroyed");
    }
}

// 设置API凭据
int rest_client_set_credentials(struct rest_client* client, const char* api_key, const char* api_secret) {
    if (!client || !client->initialized.load()) {
        spdlog::error("Invalid REST client");
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(client->mutex);
    
    if (api_key) {
        client->api_key = api_key;
    }
    
    if (api_secret) {
        client->api_secret = api_secret;
    }
    
    spdlog::debug("REST client credentials set");
    return 0;
}

// 执行GET请求
static int rest_client_get(struct rest_client* client, const char* endpoint, std::string& response) {
    if (!client || !client->initialized.load() || !endpoint) {
        spdlog::error("Invalid parameters for REST GET request");
        return -1;
    }
    
    std::string url = client->base_url + endpoint;
    response.clear();
    
    curl_easy_setopt(client->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, rest_write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, client->timeout);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    CURLcode res = curl_easy_perform(client->curl);
    if (res != CURLE_OK) {
        spdlog::error("CURL request failed: {}", curl_easy_strerror(res));
        return -1;
    }
    
    long http_code = 0;
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        spdlog::error("HTTP request failed with code: {}", http_code);
        return -1;
    }
    
    spdlog::debug("REST GET request successful: {}", url);
    return 0;
}

// 获取市场数据（示例）
int rest_client_get_market_data(struct rest_client* client, symbol_t symbol, orderbook_t* orderbook) {
    if (!client || !client->initialized.load() || !orderbook) {
        spdlog::error("Invalid parameters for get_market_data");
        return -1;
    }
    
    std::string response;
    std::string endpoint = "/api/v3/ticker/price?symbol=BTCUSDT"; // 示例端点
    
    if (rest_client_get(client, endpoint.c_str(), response) != 0) {
        return -1;
    }
    
    // 解析JSON响应
    try {
        json jobj = json::parse(response);
        if (jobj.contains("price")) {
            double price = jobj["price"].get<double>();
            
            // 填充订单薄数据
            memset(orderbook, 0, sizeof(orderbook_t));
            orderbook->symbol = symbol;
            orderbook->bid_count = 1;
            orderbook->ask_count = 1;
            orderbook->bids[0].price = price - 5.0;
            orderbook->bids[0].quantity = 1.0;
            orderbook->asks[0].price = price + 5.0;
            orderbook->asks[0].quantity = 1.0;
            orderbook->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            spdlog::debug("Market data retrieved: price={:.2f}", price);
            return 0;
        }
    } catch (const json::parse_error& e) {
        spdlog::error("JSON parse error: {}", e.what());
    } catch (const json::type_error& e) {
        spdlog::error("JSON type error: {}", e.what());
    }
    
    spdlog::error("Failed to parse market data JSON");
    return -1;
}

// 将symbol_t转换为币安交易对符号
static std::string symbol_to_binance_symbol(symbol_t symbol) {
    switch (symbol) {
        case SYMBOL_BTC_USDT: return "BTCUSDT";
        case SYMBOL_ETH_USDT: return "ETHUSDT";
        case SYMBOL_BTC_ETH: return "BTCETH";
        default: return "BTCUSDT";
    }
}

// 获取订单薄数据
int rest_client_get_orderbook(struct rest_client* client, symbol_t symbol, orderbook_t* orderbook) {
    if (!client || !client->initialized.load() || !orderbook) {
        spdlog::error("Invalid parameters for get_orderbook");
        return -1;
    }
    
    std::string binance_symbol = symbol_to_binance_symbol(symbol);
    std::string response;
    std::string endpoint = "/api/v3/depth?symbol=" + binance_symbol + "&limit=20";
    
    if (rest_client_get(client, endpoint.c_str(), response) != 0) {
        return -1;
    }
    
    // 解析JSON响应
    try {
        json jobj = json::parse(response);
        
        // 解析买盘
        if (jobj.contains("bids") && jobj["bids"].is_array()) {
            uint32_t bid_count = static_cast<uint32_t>(jobj["bids"].size());
            orderbook->bid_count = (bid_count < 20) ? bid_count : 20;
            
            for (uint32_t i = 0; i < orderbook->bid_count; i++) {
                if (jobj["bids"][i].is_array() && jobj["bids"][i].size() >= 2) {
                    orderbook->bids[i].price = std::stod(jobj["bids"][i][0].get<std::string>());
                    orderbook->bids[i].quantity = std::stod(jobj["bids"][i][1].get<std::string>());
                }
            }
        }
        
        // 解析卖盘
        if (jobj.contains("asks") && jobj["asks"].is_array()) {
            uint32_t ask_count = static_cast<uint32_t>(jobj["asks"].size());
            orderbook->ask_count = (ask_count < 20) ? ask_count : 20;
            
            for (uint32_t i = 0; i < orderbook->ask_count; i++) {
                if (jobj["asks"][i].is_array() && jobj["asks"][i].size() >= 2) {
                    orderbook->asks[i].price = std::stod(jobj["asks"][i][0].get<std::string>());
                    orderbook->asks[i].quantity = std::stod(jobj["asks"][i][1].get<std::string>());
                }
            }
        }
        
        orderbook->symbol = symbol;
        orderbook->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        spdlog::debug("Orderbook data retrieved: {} bids, {} asks", 
                     orderbook->bid_count, orderbook->ask_count);
        return 0;
    } catch (const json::parse_error& e) {
        spdlog::error("JSON parse error: {}", e.what());
    } catch (const json::type_error& e) {
        spdlog::error("JSON type error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error parsing orderbook: {}", e.what());
    }
    
    spdlog::error("Failed to parse orderbook JSON");
    return -1;
}
