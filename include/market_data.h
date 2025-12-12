#ifndef MARKET_DATA_C_H
#define MARKET_DATA_C_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// C兼容的市场数据类型定义
typedef enum {
    MARKET_DATA_TICKER = 0,
    MARKET_DATA_ORDERBOOK,
    MARKET_DATA_TRADE,
    MARKET_DATA_KLINE
} market_data_type_t;

// 交易对类型
typedef enum {
    SYMBOL_BTC_USDT = 0,
    SYMBOL_ETH_USDT,
    SYMBOL_BTC_ETH
} symbol_t;

// 价格和数量结构（网络传输用，1字节对齐，无填充）
#ifdef _MSC_VER
    #pragma pack(push, 1)
    typedef struct {
        double price;        // 8 bytes
        double quantity;    // 8 bytes
        uint64_t timestamp; // 8 bytes
    } price_level_net_t;
    #pragma pack(pop)
#else
    typedef struct __attribute__((packed)) {
        double price;        // 8 bytes
        double quantity;    // 8 bytes
        uint64_t timestamp; // 8 bytes
    } price_level_net_t;
#endif

// 价格和数量结构（本地使用）
typedef struct {
    double price;
    double quantity;
    uint64_t timestamp;
} price_level_t;

// 订单薄结构（网络传输用，1字节对齐，无填充）
#ifdef _MSC_VER
    #pragma pack(push, 1)
    typedef struct {
        uint8_t symbol;              // 1 byte
        uint8_t reserved[3];         // 3 bytes padding for alignment
        uint32_t bid_count;          // 4 bytes
        uint32_t ask_count;          // 4 bytes
        uint64_t timestamp;          // 8 bytes
        price_level_net_t bids[20];  // 20 * 24 = 480 bytes
        price_level_net_t asks[20];  // 20 * 24 = 480 bytes
    } orderbook_net_t;
    #pragma pack(pop)
#else
    typedef struct __attribute__((packed)) {
        uint8_t symbol;              // 1 byte
        uint8_t reserved[3];         // 3 bytes padding for alignment
        uint32_t bid_count;          // 4 bytes
        uint32_t ask_count;          // 4 bytes
        uint64_t timestamp;          // 8 bytes
        price_level_net_t bids[20];  // 20 * 24 = 480 bytes
        price_level_net_t asks[20];  // 20 * 24 = 480 bytes
    } orderbook_net_t;
#endif

// 订单薄结构（本地使用） todo 将订单薄深度改成可配置
typedef struct {
    symbol_t symbol;
    price_level_t bids[20];  // 买盘
    price_level_t asks[20];  // 卖盘
    uint32_t bid_count;
    uint32_t ask_count;
    uint64_t timestamp;
} orderbook_t;

// 市场数据回调函数类型
typedef void (*market_data_callback_t)(const orderbook_t* orderbook, void* user_data);

// 市场数据获取器结构
typedef struct market_data_fetcher market_data_fetcher_t;

// 创建市场数据获取器
market_data_fetcher_t* market_data_fetcher_create(void);

// 销毁市场数据获取器
void market_data_fetcher_destroy(market_data_fetcher_t* fetcher);

// 设置回调函数
int market_data_fetcher_set_callback(market_data_fetcher_t* fetcher, 
                                   market_data_callback_t callback, 
                                   void* user_data);

// 开始获取数据
int market_data_fetcher_start(market_data_fetcher_t* fetcher, symbol_t symbol);

// 停止获取数据
int market_data_fetcher_stop(market_data_fetcher_t* fetcher);

// 获取最新订单薄
int market_data_fetcher_get_orderbook(market_data_fetcher_t* fetcher, 
                                    symbol_t symbol, 
                                    orderbook_t* orderbook);

// 设置API密钥（可选）
int market_data_fetcher_set_api_key(market_data_fetcher_t* fetcher, 
                                   const char* api_key, 
                                   const char* api_secret);

// 设置数据源
int market_data_fetcher_set_data_sources(market_data_fetcher_t* fetcher, 
                                        int use_binance, 
                                        int use_coingecko);

// 网络字节序转换函数
void orderbook_to_net(const orderbook_t* local, orderbook_net_t* net);
void orderbook_from_net(const orderbook_net_t* net, orderbook_t* local);

// REST客户端函数（用于从币安获取真实数据）
struct rest_client;
struct rest_client* rest_client_create(const char* base_url);
void rest_client_destroy(struct rest_client* client);
int rest_client_get_orderbook(struct rest_client* client, symbol_t symbol, orderbook_t* orderbook);

#ifdef __cplusplus
}
#endif

#endif // MARKET_DATA_C_H

