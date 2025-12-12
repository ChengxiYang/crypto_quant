#ifndef EXECUTION_H
#define EXECUTION_H

#include <stdint.h>
#include <stdbool.h>
#include "market_data.h"

#ifdef __cplusplus
#endif

// 执行状态
typedef enum {
    EXECUTION_STATUS_IDLE = 0,
    EXECUTION_STATUS_CONNECTING,
    EXECUTION_STATUS_CONNECTED,
    EXECUTION_STATUS_DISCONNECTED,
    EXECUTION_STATUS_ERROR
} execution_status_t;

// 执行结果状态
typedef enum {
    EXECUTION_RESULT_SUCCESS = 0,
    EXECUTION_RESULT_FAILED,
    EXECUTION_RESULT_PARTIAL
} execution_result_status_t;

// 风险参数结构
typedef struct {
    double max_position_size;
    double max_daily_loss;
    double max_order_size;
    int max_orders_per_minute;
} risk_params_t;

// 执行结果结构
typedef struct {
    execution_result_status_t status;
    uint64_t order_id;
    double filled_quantity;
    double average_price;
    char error_message[256];
} execution_result_t;

// 订单执行器函数声明
int order_executor_init(void);
void order_executor_cleanup(void);
int order_executor_set_risk_params(const risk_params_t* params);
int order_executor_set_api_credentials(const char* api_key, const char* api_secret);
int order_executor_connect(void);
int order_executor_disconnect(void);
int order_executor_get_status(void);
int order_executor_submit_order(symbol_t symbol, int side, double price, double quantity, execution_result_t* result);
int order_executor_cancel_order(uint64_t order_id);
int order_executor_get_balance(symbol_t symbol, double* balance);
int order_executor_get_position(symbol_t symbol, double* position);
int order_executor_get_order_status(uint64_t order_id, execution_result_t* result);
int order_executor_get_order_history(uint64_t* order_ids, int max_count, int* actual_count);

#endif // EXECUTION_H