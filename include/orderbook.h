#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <stdint.h>
#include <stdbool.h>
#include "market_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// 订单类型
typedef enum {
    ORDER_TYPE_LIMIT = 0,
    ORDER_TYPE_MARKET,
    ORDER_TYPE_STOP_LIMIT
} order_type_t;

// 订单方向
typedef enum {
    ORDER_SIDE_BUY = 0,
    ORDER_SIDE_SELL
} order_side_t;

// 订单状态
typedef enum {
    ORDER_STATUS_PENDING = 0,
    ORDER_STATUS_PARTIALLY_FILLED,
    ORDER_STATUS_FILLED,
    ORDER_STATUS_CANCELLED,
    ORDER_STATUS_REJECTED
} order_status_t;

// 订单结构
typedef struct {
    uint64_t order_id;
    symbol_t symbol;
    order_type_t type;
    order_side_t side;
    double price;
    double quantity;
    double filled_quantity;
    order_status_t status;
    uint64_t timestamp;
    uint64_t update_time;
} order_t;

// 订单薄管理器函数声明
int orderbook_manager_init(void);
void orderbook_manager_cleanup(void);
int orderbook_manager_update(const orderbook_t* orderbook);
double orderbook_manager_get_best_bid(symbol_t symbol);
double orderbook_manager_get_best_ask(symbol_t symbol);
double orderbook_manager_get_mid_price(symbol_t symbol);
double orderbook_manager_get_spread(symbol_t symbol);
double orderbook_manager_get_depth(symbol_t symbol, int side, double price_range);
int orderbook_manager_get_orderbook(symbol_t symbol, orderbook_t* orderbook);

#ifdef __cplusplus
}
#endif

#endif // ORDERBOOK_H