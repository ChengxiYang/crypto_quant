#ifndef STRATEGY_H
#define STRATEGY_H

#include <stdint.h>
#include <stdbool.h>
#include "market_data.h"

#ifdef __cplusplus
#endif

// 交易信号类型
typedef enum {
    SIGNAL_TYPE_NONE = 0,
    SIGNAL_TYPE_BUY,
    SIGNAL_TYPE_SELL,
    SIGNAL_TYPE_HOLD
} signal_type_t;

// 策略类型
typedef enum {
    STRATEGY_TYPE_MEAN_REVERSION = 0,
    STRATEGY_TYPE_MOMENTUM,
    STRATEGY_TYPE_ARBITRAGE,
    STRATEGY_TYPE_GRID_TRADING,
    STRATEGY_TYPE_DCA,
    STRATEGY_TYPE_BREAKOUT,
    STRATEGY_TYPE_RSI_STRATEGY,
    STRATEGY_TYPE_BOLLINGER_BANDS
} strategy_type_t;

// 策略状态
typedef enum {
    STRATEGY_STATUS_STOPPED = 0,
    STRATEGY_STATUS_RUNNING,
    STRATEGY_STATUS_PAUSED
} strategy_status_t;

// 交易信号结构
typedef struct {
    signal_type_t type;
    symbol_t symbol;
    double price;
    double quantity;
    double confidence;
    char reason[256];
    uint64_t timestamp;
} trading_signal_t;

// 策略参数结构
typedef struct {
    strategy_type_t strategy_type;
    double risk_per_trade;
    double max_position_size;
    int lookback_period;
    double z_score_threshold;
    int mean_period;
    int short_period;
    int long_period;
    double momentum_threshold;
    int rsi_period;
    double rsi_oversold;
    double rsi_overbought;
    int bb_period;
    double bb_std_dev;
    double grid_spacing;
    int grid_levels;
} strategy_params_t;

// 策略引擎函数声明
int strategy_engine_init(void);
void strategy_engine_cleanup(void);
int strategy_engine_set_params(const strategy_params_t* params);
int strategy_engine_start(void);
int strategy_engine_stop(void);
int strategy_engine_process_market_data(const orderbook_t* orderbook);

// 策略执行函数
int strategy_engine_execute_mean_reversion(const orderbook_t* orderbook, int symbol_index);
int strategy_engine_execute_momentum(const orderbook_t* orderbook, int symbol_index);
int strategy_engine_execute_rsi(const orderbook_t* orderbook, int symbol_index);

// 工具函数
double calculate_rsi(const double* prices, int count, int period);

#endif // STRATEGY_H