#include "market_data.h"
#include "network_utils.h"
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

// 将本地订单薄转换为网络字节序
void orderbook_to_net(const orderbook_t* local, orderbook_net_t* net) {
    if (!local || !net) {
        return;
    }
    
    memset(net, 0, sizeof(orderbook_net_t));
    
    // 转换基本字段
    net->symbol = static_cast<uint8_t>(local->symbol);
    net->bid_count = hton32(local->bid_count);
    net->ask_count = hton32(local->ask_count);
    net->timestamp = hton64(local->timestamp);
    
    // 转换买盘数据
    for (uint32_t i = 0; i < local->bid_count && i < 20; ++i) {
        net->bids[i].price = hton_double(local->bids[i].price);
        net->bids[i].quantity = hton_double(local->bids[i].quantity);
        net->bids[i].timestamp = hton64(local->bids[i].timestamp);
    }
    
    // 转换卖盘数据
    for (uint32_t i = 0; i < local->ask_count && i < 20; ++i) {
        net->asks[i].price = hton_double(local->asks[i].price);
        net->asks[i].quantity = hton_double(local->asks[i].quantity);
        net->asks[i].timestamp = hton64(local->asks[i].timestamp);
    }
}

// 将网络字节序订单薄转换为本地格式
void orderbook_from_net(const orderbook_net_t* net, orderbook_t* local) {
    if (!net || !local) {
        return;
    }
    
    memset(local, 0, sizeof(orderbook_t));
    
    // 转换基本字段
    local->symbol = static_cast<symbol_t>(net->symbol);
    local->bid_count = ntoh32(net->bid_count);
    local->ask_count = ntoh32(net->ask_count);
    local->timestamp = ntoh64(net->timestamp);
    
    // 转换买盘数据
    for (uint32_t i = 0; i < local->bid_count && i < 20; ++i) {
        local->bids[i].price = ntoh_double(net->bids[i].price);
        local->bids[i].quantity = ntoh_double(net->bids[i].quantity);
        local->bids[i].timestamp = ntoh64(net->bids[i].timestamp);
    }
    
    // 转换卖盘数据
    for (uint32_t i = 0; i < local->ask_count && i < 20; ++i) {
        local->asks[i].price = ntoh_double(net->asks[i].price);
        local->asks[i].quantity = ntoh_double(net->asks[i].quantity);
        local->asks[i].timestamp = ntoh64(net->asks[i].timestamp);
    }
}

#ifdef __cplusplus
}
#endif

