#include "orderbook_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>

namespace crypto_quant {

OrderbookManager::OrderbookManager() {
        orderbooks_.resize(3);
        // 初始化每个订单薄
        for (auto& orderbook : orderbooks_) {
            memset(&orderbook, 0, sizeof(orderbook));
            orderbook.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
    }

bool OrderbookManager::initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::info("OrderbookManager initialized");
        return true;
    }

void OrderbookManager::cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        orderbooks_.clear();
        spdlog::info("OrderbookManager cleaned up");
    }

void OrderbookManager::updateOrderbook(const orderbook_t& orderbook) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(orderbook.symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            spdlog::error("Invalid symbol index: {}", symbol_index);
            return;
        }
        
        // 更新订单薄数据
        orderbooks_[symbol_index] = orderbook;
        
        spdlog::debug("Orderbook updated: symbol={}, bid_count={}, ask_count={}, timestamp={}",
                    static_cast<int>(orderbook.symbol), orderbook.bid_count, 
                    orderbook.ask_count, orderbook.timestamp);
    }

orderbook_t OrderbookManager::getOrderbook(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            spdlog::error("Invalid symbol index: {}", symbol_index);
            return orderbook_t{};
        }
        
        return orderbooks_[symbol_index];
    }

double OrderbookManager::getBestBid(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return 0.0;
        }
        
        const auto& orderbook = orderbooks_[symbol_index];
        if (orderbook.bid_count > 0) {
            return orderbook.bids[0].price;
        }
        
        return 0.0;
    }

double OrderbookManager::getBestAsk(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return 0.0;
        }
        
        const auto& orderbook = orderbooks_[symbol_index];
        if (orderbook.ask_count > 0) {
            return orderbook.asks[0].price;
        }
        
        return 0.0;
    }

double OrderbookManager::getMidPrice(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return 0.0;
        }
        
        const auto& orderbook = orderbooks_[symbol_index];
        if (orderbook.bid_count > 0 && orderbook.ask_count > 0) {
            return (orderbook.bids[0].price + orderbook.asks[0].price) / 2.0;
        }
        
        return 0.0;
    }

double OrderbookManager::getSpread(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return 0.0;
        }
        
        const auto& orderbook = orderbooks_[symbol_index];
        if (orderbook.bid_count > 0 && orderbook.ask_count > 0) {
            return orderbook.asks[0].price - orderbook.bids[0].price;
        }
        
        return 0.0;
    }

double OrderbookManager::getBidDepth(symbol_t symbol, int levels) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return 0.0;
        }
        
        const auto& orderbook = orderbooks_[symbol_index];
        double depth = 0.0;
        
        int count = std::min(levels, static_cast<int>(orderbook.bid_count));
        for (int i = 0; i < count; ++i) {
            depth += orderbook.bids[i].quantity;
        }
        
        return depth;
    }

double OrderbookManager::getAskDepth(symbol_t symbol, int levels) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return 0.0;
        }
        
        const auto& orderbook = orderbooks_[symbol_index];
        double depth = 0.0;
        
        int count = std::min(levels, static_cast<int>(orderbook.ask_count));
        for (int i = 0; i < count; ++i) {
            depth += orderbook.asks[i].quantity;
        }
        
        return depth;
    }

uint64_t OrderbookManager::getTimestamp(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return 0;
        }
        
        return orderbooks_[symbol_index].timestamp;
    }

bool OrderbookManager::isValid(symbol_t symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int symbol_index = static_cast<int>(symbol);
        if (symbol_index < 0 || symbol_index >= static_cast<int>(orderbooks_.size())) {
            return false;
        }
        
        const auto& orderbook = orderbooks_[symbol_index];
        
        // 检查是否有有效的买卖盘
        if (orderbook.bid_count > 0 && orderbook.ask_count > 0) {
            // 检查价格是否合理
            if (orderbook.bids[0].price > 0 && orderbook.asks[0].price > 0) {
                return true; // 有效
            }
        }
        
        return false; // 无效
    }
}