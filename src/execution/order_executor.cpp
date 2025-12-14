#include "order_execution.h"
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>

using json = nlohmann::json;

namespace crypto_quant
{

    // 币安API端点
    static const std::string BINANCE_BASE_URL = "https://api.binance.com";
    static const std::string BINANCE_TESTNET_URL = "https://testnet.binance.vision/api";

    // 将symbol_t转换为币安交易对符号
    static std::string symbol_to_binance(symbol_t symbol)
    {
        switch (symbol)
        {
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

    OrderExecutor::OrderExecutor() : status_(ExecutionStatus::IDLE), next_order_id_(1)
    {
        base_url_ = BINANCE_BASE_URL;
    }

    bool OrderExecutor::initialize()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = ExecutionStatus::IDLE;
        next_order_id_.store(1);
        spdlog::info("OrderExecutor initialized");
        return true;
    }

    void OrderExecutor::cleanup()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        order_history_.clear();
        status_ = ExecutionStatus::IDLE;
        spdlog::info("OrderExecutor cleaned up");
    }

    void OrderExecutor::setRiskParams(const RiskParams &params)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        risk_params_ = params;
        spdlog::info("Risk parameters updated: max_position={}, max_loss={}, max_order={}, max_orders_per_min={}",
                     params.max_position_size, params.max_daily_loss,
                     params.max_order_size, params.max_orders_per_minute);
    }

    void OrderExecutor::setApiCredentials(const std::string &api_key, const std::string &api_secret)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        api_key_ = api_key;
        api_secret_ = api_secret;
        spdlog::info("API credentials set");
    }

    bool OrderExecutor::connect()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (api_key_.empty() || api_secret_.empty())
        {
            spdlog::error("API credentials not set");
            status_ = ExecutionStatus::ERROR;
            return false;
        }

        status_ = ExecutionStatus::CONNECTING;

        // 测试连接：获取账户信息
        std::string response = send_signed_request("GET", "/api/v3/account", "");

        try
        {
            json j = json::parse(response);
            if (j.contains("accountType"))
            {
                status_ = ExecutionStatus::CONNECTED;
                spdlog::info("Connected to Binance API successfully");
                return true;
            }
            else if (j.contains("code"))
            {
                spdlog::error("Binance API error: {} - {}", j.value("code", -1), j.value("msg", "Unknown error"));
                status_ = ExecutionStatus::ERROR;
                return false;
            }
        }
        catch (const json::parse_error &e)
        {
            spdlog::error("Failed to parse account response: {}", e.what());
            status_ = ExecutionStatus::ERROR;
            return false;
        }

        status_ = ExecutionStatus::ERROR;
        return false;
    }

    void OrderExecutor::disconnect()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = ExecutionStatus::DISCONNECTED;
        spdlog::info("Disconnected from exchange");
    }

    ExecutionStatus OrderExecutor::getStatus() const
    {
        return status_.load();
    }

    ExecutionResult OrderExecutor::submitOrder(symbol_t symbol, int side, double price, double quantity)
    {
        ExecutionResult result;
        result.status = ExecutionResultStatus::FAILED;

        if (status_ != ExecutionStatus::CONNECTED)
        {
            result.error_message = "Not connected to exchange";
            spdlog::error("Order submission failed: {}", result.error_message);
            return result;
        }

        // 风险检查
        std::lock_guard<std::mutex> lock(mutex_);

        if (quantity > risk_params_.max_order_size)
        {
            result.error_message = "Order size exceeds maximum allowed";
            spdlog::error("Order submission failed: {}", result.error_message);
            return result;
        }

        // 构建订单参数
        std::string binance_symbol = symbol_to_binance(symbol);
        std::string side_str = (side == 0) ? "BUY" : "SELL"; // 0=BUY, 1=SELL
        std::string type_str = (price > 0) ? "LIMIT" : "MARKET";

        std::stringstream query;
        query << "symbol=" << binance_symbol
              << "&side=" << side_str
              << "&type=" << type_str
              << "&quantity=" << std::fixed << std::setprecision(8) << quantity;

        if (type_str == "LIMIT")
        {
            query << "&timeInForce=GTC"
                  << "&price=" << std::fixed << std::setprecision(8) << price;
        }

        // 发送下单请求
        std::string response = send_signed_request("POST", "/api/v3/order", query.str());

        try
        {
            json j = json::parse(response);

            if (j.contains("orderId"))
            {
                // 订单创建成功
                uint64_t order_id = j["orderId"].get<uint64_t>();

                result.status = ExecutionResultStatus::SUCCESS;
                result.order_id = order_id;
                result.filled_quantity = j.value("executedQty", 0.0);
                result.average_price = j.value("price", price);

                if (j.contains("status"))
                {
                    std::string status = j["status"].get<std::string>();
                    if (status == "FILLED")
                    {
                        result.filled_quantity = quantity;
                        result.average_price = j.value("price", price);
                    }
                    else if (status == "PARTIALLY_FILLED")
                    {
                        result.status = ExecutionResultStatus::PARTIAL;
                        result.filled_quantity = j.value("executedQty", 0.0);
                    }
                }

                // 记录订单历史
                order_history_[order_id] = result;

                spdlog::info("Order submitted successfully: id={}, symbol={}, side={}, price={:.2f}, quantity={:.2f}",
                             order_id, binance_symbol, side_str, price, quantity);
            }
            else if (j.contains("code"))
            {
                // API错误
                result.error_message = j.value("msg", "Unknown error");
                spdlog::error("Order submission failed: {} - {}", j.value("code", -1), result.error_message);
            }
            else
            {
                result.error_message = "Invalid response from exchange";
                spdlog::error("Order submission failed: {}", result.error_message);
            }
        }
        catch (const json::parse_error &e)
        {
            result.error_message = "Failed to parse response: " + std::string(e.what());
            spdlog::error("Order submission failed: {}", result.error_message);
        }
        catch (const std::exception &e)
        {
            result.error_message = "Exception: " + std::string(e.what());
            spdlog::error("Order submission failed: {}", result.error_message);
        }

        return result;
    }

    bool OrderExecutor::cancelOrder(uint64_t order_id)
    {
        if (status_ != ExecutionStatus::CONNECTED)
        {
            spdlog::error("Cannot cancel order: not connected to exchange");
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 查找订单以获取交易对信息
        auto it = order_history_.find(order_id);
        if (it == order_history_.end())
        {
            spdlog::warn("Order not found in history: id={}", order_id);
            // 仍然尝试取消，使用默认交易对
        }

        // 构建取消订单参数（需要symbol和orderId）
        // 注意：币安API需要symbol，但我们只有orderId，所以需要先查询订单
        ExecutionResult order_info = getOrderStatus(order_id);
        if (order_info.status == ExecutionResultStatus::FAILED)
        {
            spdlog::error("Cannot cancel order: order not found");
            return false;
        }

        // 从订单历史中获取symbol（这里简化处理，实际应该存储symbol）
        std::string binance_symbol = "BTCUSDT"; // 默认值，实际应该从订单信息中获取

        std::stringstream query;
        query << "symbol=" << binance_symbol
              << "&orderId=" << order_id;

        std::string response = send_signed_request("DELETE", "/api/v3/order", query.str());

        try
        {
            json j = json::parse(response);

            if (j.contains("orderId"))
            {
                // 更新订单状态
                if (it != order_history_.end())
                {
                    it->second.status = ExecutionResultStatus::FAILED;
                    it->second.error_message = "Order cancelled";
                }
                spdlog::info("Order cancelled successfully: id={}", order_id);
                return true;
            }
            else if (j.contains("code"))
            {
                spdlog::error("Cancel order failed: {} - {}", j.value("code", -1), j.value("msg", "Unknown error"));
                return false;
            }
        }
        catch (const json::parse_error &e)
        {
            spdlog::error("Failed to parse cancel response: {}", e.what());
            return false;
        }

        return false;
    }

    double OrderExecutor::getBalance(symbol_t symbol)
    {
        if (status_ != ExecutionStatus::CONNECTED)
        {
            spdlog::error("Cannot get balance: not connected to exchange");
            return 0.0;
        }

        std::string response = send_signed_request("GET", "/api/v3/account", "");

        try
        {
            json j = json::parse(response);

            if (j.contains("balances") && j["balances"].is_array())
            {
                std::string asset;
                switch (symbol)
                {
                case SYMBOL_BTC_USDT:
                case SYMBOL_BTC_ETH:
                    asset = "BTC";
                    break;
                case SYMBOL_ETH_USDT:
                    asset = "ETH";
                    break;
                default:
                    asset = "USDT";
                }

                for (const auto &balance : j["balances"])
                {
                    if (balance.contains("asset") && balance["asset"].get<std::string>() == asset)
                    {
                        double free = std::stod(balance["free"].get<std::string>());
                        spdlog::debug("Balance query: asset={}, balance={:.8f}", asset, free);
                        return free;
                    }
                }
            }
        }
        catch (const json::parse_error &e)
        {
            spdlog::error("Failed to parse balance response: {}", e.what());
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error getting balance: {}", e.what());
        }

        return 0.0;
    }

    double OrderExecutor::getPosition(symbol_t /*symbol*/)
    {
        // 币安现货交易没有持仓概念，返回0
        // 如果是合约交易，需要调用不同的API
        return 0.0;
    }

    ExecutionResult OrderExecutor::getOrderStatus(uint64_t order_id)
    {
        ExecutionResult result;
        result.status = ExecutionResultStatus::FAILED;

        if (status_ != ExecutionStatus::CONNECTED)
        {
            result.error_message = "Not connected to exchange";
            return result;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 先检查本地历史
        auto it = order_history_.find(order_id);
        if (it != order_history_.end())
        {
            // 从交易所查询最新状态
            std::string binance_symbol = "BTCUSDT"; // 简化处理

            std::stringstream query;
            query << "symbol=" << binance_symbol
                  << "&orderId=" << order_id;

            std::string response = send_signed_request("GET", "/api/v3/order", query.str());

            try
            {
                json j = json::parse(response);

                if (j.contains("orderId"))
                {
                    result.order_id = j["orderId"].get<uint64_t>();
                    result.filled_quantity = std::stod(j.value("executedQty", "0"));

                    std::string status = j.value("status", "");
                    if (status == "FILLED")
                    {
                        result.status = ExecutionResultStatus::SUCCESS;
                        result.average_price = std::stod(j.value("price", "0"));
                    }
                    else if (status == "PARTIALLY_FILLED")
                    {
                        result.status = ExecutionResultStatus::PARTIAL;
                        result.average_price = std::stod(j.value("price", "0"));
                    }
                    else if (status == "CANCELED" || status == "REJECTED")
                    {
                        result.status = ExecutionResultStatus::FAILED;
                        result.error_message = "Order " + status;
                    }
                    else
                    {
                        result.status = ExecutionResultStatus::FAILED;
                    }

                    // 更新本地历史
                    it->second = result;
                }
                else if (j.contains("code"))
                {
                    result.error_message = j.value("msg", "Unknown error");
                }
            }
            catch (const json::parse_error &e)
            {
                spdlog::error("Failed to parse order status response: {}", e.what());
                result.error_message = "Parse error";
            }
        }
        else
        {
            result.error_message = "Order not found";
            spdlog::warn("Order not found: id={}", order_id);
        }

        return result;
    }

    std::vector<uint64_t> OrderExecutor::getOrderHistory(int max_count)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<uint64_t> order_ids;
        order_ids.reserve(std::min(max_count, static_cast<int>(order_history_.size())));

        for (const auto &pair : order_history_)
        {
            if (static_cast<int>(order_ids.size()) >= max_count)
            {
                break;
            }
            order_ids.push_back(pair.first);
        }

        spdlog::debug("Order history query: returned {} orders", order_ids.size());
        return order_ids;
    }

    // HMAC SHA256 签名生成
    std::string OrderExecutor::hmac_sha256(const std::string &key, const std::string &data)
    {
        unsigned char *digest = HMAC(
            EVP_sha256(),
            key.c_str(),
            static_cast<int>(key.length()),
            reinterpret_cast<const unsigned char *>(data.c_str()),
            data.length(),
            nullptr,
            nullptr);

        if (digest == nullptr)
        {
            return "";
        }

        std::stringstream ss;
        for (int i = 0; i < 32; ++i)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }

        OPENSSL_free(digest);
        return ss.str();
    }

    // 获取毫秒级时间戳
    long long OrderExecutor::get_current_ms()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    // Libcurl 写入回调
    size_t OrderExecutor::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        ((std::string *)userp)->append((char *)contents, size * nmemb);
        return size * nmemb;
    }

    // 发送签名请求
    std::string OrderExecutor::send_signed_request(const std::string &method, const std::string &endpoint, const std::string &query_string)
    {
        CURL *curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (!curl)
        {
            spdlog::error("Failed to initialize CURL");
            return "";
        }

        // 构建完整查询字符串（添加时间戳和签名）
        std::string full_query = query_string;
        if (!full_query.empty())
        {
            full_query += "&";
        }
        full_query += "timestamp=" + std::to_string(get_current_ms());

        // 生成签名
        std::string signature = hmac_sha256(api_secret_, full_query);
        full_query += "&signature=" + signature;

        std::string url = base_url_ + endpoint + "?" + full_query;

        // 构建Header
        struct curl_slist *headers = nullptr;
        std::string key_header = "X-MBX-APIKEY: " + api_key_;
        headers = curl_slist_append(headers, key_header.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        if (method == "POST")
        {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        }
        else if (method == "DELETE")
        {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }

        res = curl_easy_perform(curl);

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res != CURLE_OK)
        {
            spdlog::error("CURL request failed: {}", curl_easy_strerror(res));
        }
        else if (http_code != 200)
        {
            spdlog::warn("HTTP response code: {}", http_code);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return readBuffer;
    }
}
