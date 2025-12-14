#ifndef ORDER_EXECUTION_H
#define ORDER_EXECUTION_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "crypto_quant.h"

namespace crypto_quant {

// 订单执行器实现类
class OrderExecutor : public IOrderExecutor {
private:
    RiskParams risk_params_;
    std::atomic<ExecutionStatus> status_;
    std::string base_url_;
    std::string api_key_;
    std::string api_secret_;
    std::atomic<uint64_t> next_order_id_;
    std::unordered_map<uint64_t, ExecutionResult> order_history_;
    mutable std::mutex mutex_;

public:
    OrderExecutor();

    bool initialize() override;
    void cleanup() override;
    void setRiskParams(const RiskParams& params) override;
    void setApiCredentials(const std::string& api_key, const std::string& api_secret) override;
    bool connect() override;
    void disconnect() override;
    ExecutionStatus getStatus() const override;
    ExecutionResult submitOrder(symbol_t symbol, int side, double price, double quantity) override;
    bool cancelOrder(uint64_t order_id) override;
    double getBalance(symbol_t symbol) override;
    double getPosition(symbol_t symbol) override;
    ExecutionResult getOrderStatus(uint64_t order_id) override;
    std::vector<uint64_t> getOrderHistory(int max_count = 100) override;

private:
    // HMAC SHA256 签名生成
    std::string hmac_sha256(const std::string& key, const std::string& data);
    
    // 获取毫秒级时间戳
    long long get_current_ms();
    
    // Libcurl 写入回调
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    
    // 发送签名请求
    std::string send_signed_request(const std::string& method, const std::string& endpoint, const std::string& query_string);
};

} // namespace crypto_quant

#endif // ORDER_EXECUTION_H
