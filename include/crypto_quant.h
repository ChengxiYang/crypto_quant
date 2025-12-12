#ifndef CRYPTO_QUANT_H
#define CRYPTO_QUANT_H

// 版本信息
#define CRYPTO_QUANT_VERSION_MAJOR 1
#define CRYPTO_QUANT_VERSION_MINOR 0
#define CRYPTO_QUANT_VERSION_PATCH 0
#define CRYPTO_QUANT_VERSION_STRING "1.0.0"

// 确保C++11兼容性
#if defined(__cplusplus) && __cplusplus >= 201103L
    #define CRYPTO_QUANT_CPP11 1
#else
    #define CRYPTO_QUANT_CPP11 0
#endif

// 确保C11兼容性
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define CRYPTO_QUANT_C11 1
#else
    #define CRYPTO_QUANT_C11 0
#endif

// 包含基础头文件
#include <stdint.h>
#include <stdbool.h>

// C++部分
#ifdef __cplusplus

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>

// 包含市场数据头文件
#include "market_data.h"

namespace crypto_quant {

// 前向声明
class StrategyEngine;
class OrderExecutor;
class OrderbookManager;
class MarketDataProvider;

// 交易信号类型
enum class SignalType {
    NONE = 0,
    BUY,
    SELL,
    HOLD
};

// 策略类型
enum class StrategyType {
    MEAN_REVERSION = 0,
    MOMENTUM,
    ARBITRAGE,
    GRID_TRADING,
    DCA,
    BREAKOUT,
    RSI_STRATEGY,
    BOLLINGER_BANDS
};

// 策略状态
enum class StrategyStatus {
    STOPPED = 0,
    RUNNING,
    PAUSED
};

// 执行状态
enum class ExecutionStatus {
    IDLE = 0,
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    ERROR
};

// 执行结果状态
enum class ExecutionResultStatus {
    SUCCESS = 0,
    FAILED,
    PARTIAL
};

// 交易信号结构
struct TradingSignal {
    SignalType type;
    symbol_t symbol;
    double price;
    double quantity;
    double confidence;
    std::string reason;
    std::chrono::milliseconds timestamp;
    
    TradingSignal() : type(SignalType::NONE), symbol(SYMBOL_BTC_USDT), 
                     price(0.0), quantity(0.0), confidence(0.0),
                     timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())) {}
};

// 策略参数结构
struct StrategyParams {
    StrategyType strategy_type;
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
    
    StrategyParams() : strategy_type(StrategyType::MEAN_REVERSION),
                      risk_per_trade(0.02), max_position_size(1000.0),
                      lookback_period(20), z_score_threshold(2.0),
                      mean_period(20), short_period(12), long_period(26),
                      momentum_threshold(0.01), rsi_period(14),
                      rsi_oversold(30.0), rsi_overbought(70.0),
                      bb_period(20), bb_std_dev(2.0),
                      grid_spacing(0.001), grid_levels(10) {}
};

// 风险参数结构
struct RiskParams {
    double max_position_size;
    double max_daily_loss;
    double max_order_size;
    int max_orders_per_minute;
    
    RiskParams() : max_position_size(10000.0), max_daily_loss(1000.0),
                   max_order_size(1000.0), max_orders_per_minute(60) {}
};

// 执行结果结构
struct ExecutionResult {
    ExecutionResultStatus status;
    uint64_t order_id;
    double filled_quantity;
    double average_price;
    std::string error_message;
    
    ExecutionResult() : status(ExecutionResultStatus::FAILED), order_id(0),
                       filled_quantity(0.0), average_price(0.0) {}
};

// 抽象策略基类
class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual SignalType processMarketData(const orderbook_t& orderbook) = 0;
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual StrategyStatus getStatus() const = 0;
    virtual void setStatus(StrategyStatus status) = 0;
    virtual void setParams(const StrategyParams& params) = 0;
    virtual StrategyParams getParams() const = 0;
};

// 策略引擎接口
class IStrategyEngine {
public:
    virtual ~IStrategyEngine() = default;
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual void setStrategy(std::shared_ptr<IStrategy> strategy) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual StrategyStatus getStatus() const = 0;
    virtual void processMarketData(const orderbook_t& orderbook) = 0;
};

// 订单执行器接口
class IOrderExecutor {
public:
    virtual ~IOrderExecutor() = default;
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual void setRiskParams(const RiskParams& params) = 0;
    virtual void setApiCredentials(const std::string& api_key, const std::string& api_secret) = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual ExecutionStatus getStatus() const = 0;
    virtual ExecutionResult submitOrder(symbol_t symbol, int side, double price, double quantity) = 0;
    virtual bool cancelOrder(uint64_t order_id) = 0;
    virtual double getBalance(symbol_t symbol) = 0;
    virtual double getPosition(symbol_t symbol) = 0;
    virtual ExecutionResult getOrderStatus(uint64_t order_id) = 0;
    virtual std::vector<uint64_t> getOrderHistory(int max_count = 100) = 0;
};

// 订单薄管理器接口
class IOrderbookManager {
public:
    virtual ~IOrderbookManager() = default;
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual void updateOrderbook(const orderbook_t& orderbook) = 0;
    virtual orderbook_t getOrderbook(symbol_t symbol) const = 0;
    virtual double getBestBid(symbol_t symbol) const = 0;
    virtual double getBestAsk(symbol_t symbol) const = 0;
    virtual double getMidPrice(symbol_t symbol) const = 0;
    virtual double getSpread(symbol_t symbol) const = 0;
    virtual double getBidDepth(symbol_t symbol, int levels = 5) const = 0;
    virtual double getAskDepth(symbol_t symbol, int levels = 5) const = 0;
    virtual uint64_t getTimestamp(symbol_t symbol) const = 0;
    virtual bool isValid(symbol_t symbol) const = 0;
};

// 市场数据提供者接口
class IMarketDataProvider {
public:
    virtual ~IMarketDataProvider() = default;
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual void setCallback(std::function<void(const orderbook_t&)> callback) = 0;
    virtual bool start(symbol_t symbol) = 0;
    virtual void stop() = 0;
    virtual orderbook_t getOrderbook(symbol_t symbol) const = 0;
    virtual void setApiKey(const std::string& api_key, const std::string& api_secret) = 0;
    virtual void setDataSources(bool use_binance, bool use_coingecko) = 0;
};

// 工厂类
class CryptoQuantFactory {
public:
    static std::shared_ptr<IStrategyEngine> createStrategyEngine();
    static std::shared_ptr<IOrderExecutor> createOrderExecutor();
    static std::shared_ptr<IOrderbookManager> createOrderbookManager();
    static std::shared_ptr<IMarketDataProvider> createMarketDataProvider();
    
    // 创建具体策略
    static std::shared_ptr<IStrategy> createMeanReversionStrategy();
    static std::shared_ptr<IStrategy> createMomentumStrategy();
    static std::shared_ptr<IStrategy> createRSIStrategy();
};

} // namespace crypto_quant

#endif // __cplusplus

// C接口部分
#ifdef __cplusplus
extern "C" {
#endif

// 获取版本信息
const char* crypto_quant_get_version(void);
const char* crypto_quant_get_version_string(void);

// 初始化库
int crypto_quant_init(void);

// 清理库
void crypto_quant_cleanup(void);

// 日志功能（使用spdlog）
void crypto_quant_log_message(const char* level, const char* message);
void crypto_quant_log_debug(const char* message);
void crypto_quant_log_info(const char* message);
void crypto_quant_log_warn(const char* message);
void crypto_quant_log_error(const char* message);
void crypto_quant_log_critical(const char* message);

// 初始化日志系统
void crypto_quant_init_logger(void);

#ifdef __cplusplus
}
#endif

#endif // CRYPTO_QUANT_H