#ifndef ORDERBOOK_MANAGER_H
#define ORDERBOOK_MANAGER_H

#include <vector>
#include <mutex>
#include <cstring>

#include "crypto_quant.h"

namespace crypto_quant {

// 订单薄管理器实现类
class OrderbookManager : public IOrderbookManager {
private:
    std::vector<orderbook_t> orderbooks_;
    mutable std::mutex mutex_;

public:
    OrderbookManager();

    bool initialize() override;
    void cleanup() override;
    void updateOrderbook(const orderbook_t& orderbook) override;
    orderbook_t getOrderbook(symbol_t symbol) const override;
    double getBestBid(symbol_t symbol) const override;
    double getBestAsk(symbol_t symbol) const override;
    double getMidPrice(symbol_t symbol) const override;
    double getSpread(symbol_t symbol) const override;
    double getBidDepth(symbol_t symbol, int levels = 5) const override;
    double getAskDepth(symbol_t symbol, int levels = 5) const override;
    uint64_t getTimestamp(symbol_t symbol) const override;
    bool isValid(symbol_t symbol) const override;
};

} // namespace crypto_quant

#endif // ORDERBOOK_MANAGER_H

