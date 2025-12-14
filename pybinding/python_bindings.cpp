#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/chrono.h>
#include "crypto_quant.h"

namespace py = pybind11;
using namespace crypto_quant;

// 市场数据模块绑定
void bind_market_data(py::module& m) {
    // 绑定 price_level_t
    py::class_<price_level_t>(m, "PriceLevel")
        .def_readwrite("price", &price_level_t::price)
        .def_readwrite("quantity", &price_level_t::quantity)
        .def_readwrite("timestamp", &price_level_t::timestamp);
    
    // 绑定 orderbook_t
    py::class_<orderbook_t>(m, "Orderbook")
        .def(py::init<>())
        .def_readwrite("symbol", &orderbook_t::symbol)
        .def("get_bids", [](const orderbook_t& self) {
            std::vector<price_level_t> bids;
            for (uint32_t i = 0; i < self.bid_count && i < 20; ++i) {
                bids.push_back(self.bids[i]);
            }
            return bids;
        })
        .def("set_bids", [](orderbook_t& self, const std::vector<price_level_t>& bids) {
            self.bid_count = static_cast<uint32_t>(std::min(bids.size(), size_t(20)));
            for (uint32_t i = 0; i < self.bid_count; ++i) {
                self.bids[i] = bids[i];
            }
        })
        .def("get_asks", [](const orderbook_t& self) {
            std::vector<price_level_t> asks;
            for (uint32_t i = 0; i < self.ask_count && i < 20; ++i) {
                asks.push_back(self.asks[i]);
            }
            return asks;
        })
        .def("set_asks", [](orderbook_t& self, const std::vector<price_level_t>& asks) {
            self.ask_count = static_cast<uint32_t>(std::min(asks.size(), size_t(20)));
            for (uint32_t i = 0; i < self.ask_count; ++i) {
                self.asks[i] = asks[i];
            }
        })
        .def_readwrite("bid_count", &orderbook_t::bid_count)
        .def_readwrite("ask_count", &orderbook_t::ask_count)
        .def_readwrite("timestamp", &orderbook_t::timestamp);
    
    // 绑定 symbol_t 枚举
    py::enum_<symbol_t>(m, "Symbol")
        .value("BTC_USDT", SYMBOL_BTC_USDT)
        .value("ETH_USDT", SYMBOL_ETH_USDT)
        .value("BTC_ETH", SYMBOL_BTC_ETH);
    
    // 绑定 IMarketDataFetcher 接口
    py::class_<IMarketDataFetcher, std::shared_ptr<IMarketDataFetcher>>(m, "MarketDataFetcher")
        .def("initialize", &IMarketDataFetcher::initialize)
        .def("start", &IMarketDataFetcher::start)
        .def("stop", &IMarketDataFetcher::stop)
        .def("set_orderbook_callback", &IMarketDataFetcher::setOrderbookCallback)
        .def("get_orderbook", &IMarketDataFetcher::getOrderbook)
        .def("set_api_key", &IMarketDataFetcher::setApiKey)
        .def("set_data_sources", &IMarketDataFetcher::setDataSources);
}

// 订单薄模块绑定
void bind_orderbook(py::module& m) {
    // 绑定 IOrderbookManager 接口
    py::class_<IOrderbookManager, std::shared_ptr<IOrderbookManager>>(m, "OrderbookManager")
        .def("initialize", &IOrderbookManager::initialize)
        .def("cleanup", &IOrderbookManager::cleanup)
        .def("update_orderbook", &IOrderbookManager::updateOrderbook)
        .def("get_orderbook", &IOrderbookManager::getOrderbook)
        .def("get_best_bid", &IOrderbookManager::getBestBid)
        .def("get_best_ask", &IOrderbookManager::getBestAsk)
        .def("get_mid_price", &IOrderbookManager::getMidPrice)
        .def("get_spread", &IOrderbookManager::getSpread)
        .def("get_bid_depth", &IOrderbookManager::getBidDepth,
             py::arg("symbol"), py::arg("levels") = 5)
        .def("get_ask_depth", &IOrderbookManager::getAskDepth,
             py::arg("symbol"), py::arg("levels") = 5)
        .def("get_timestamp", &IOrderbookManager::getTimestamp)
        .def("is_valid", &IOrderbookManager::isValid);
}

// 策略模块绑定
void bind_strategy(py::module& m) {
    // 绑定 SignalType 枚举
    py::enum_<SignalType>(m, "SignalType")
        .value("NONE", SignalType::NONE)
        .value("BUY", SignalType::BUY)
        .value("SELL", SignalType::SELL)
        .value("HOLD", SignalType::HOLD);
    
    // 绑定 StrategyType 枚举
    py::enum_<StrategyType>(m, "StrategyType")
        .value("MEAN_REVERSION", StrategyType::MEAN_REVERSION)
        .value("MOMENTUM", StrategyType::MOMENTUM)
        .value("ARBITRAGE", StrategyType::ARBITRAGE)
        .value("GRID_TRADING", StrategyType::GRID_TRADING)
        .value("DCA", StrategyType::DCA)
        .value("BREAKOUT", StrategyType::BREAKOUT)
        .value("RSI_STRATEGY", StrategyType::RSI_STRATEGY)
        .value("BOLLINGER_BANDS", StrategyType::BOLLINGER_BANDS);
    
    // 绑定 StrategyStatus 枚举
    py::enum_<StrategyStatus>(m, "StrategyStatus")
        .value("STOPPED", StrategyStatus::STOPPED)
        .value("RUNNING", StrategyStatus::RUNNING)
        .value("PAUSED", StrategyStatus::PAUSED);
    
    // 绑定 TradingSignal 结构
    py::class_<TradingSignal>(m, "TradingSignal")
        .def(py::init<>())
        .def_readwrite("type", &TradingSignal::type)
        .def_readwrite("symbol", &TradingSignal::symbol)
        .def_readwrite("price", &TradingSignal::price)
        .def_readwrite("quantity", &TradingSignal::quantity)
        .def_readwrite("confidence", &TradingSignal::confidence)
        .def_readwrite("reason", &TradingSignal::reason)
        .def_readwrite("timestamp", &TradingSignal::timestamp);
    
    // 绑定 StrategyParams 结构
    py::class_<StrategyParams>(m, "StrategyParams")
        .def(py::init<>())
        .def_readwrite("strategy_type", &StrategyParams::strategy_type)
        .def_readwrite("risk_per_trade", &StrategyParams::risk_per_trade)
        .def_readwrite("max_position_size", &StrategyParams::max_position_size)
        .def_readwrite("lookback_period", &StrategyParams::lookback_period)
        .def_readwrite("z_score_threshold", &StrategyParams::z_score_threshold)
        .def_readwrite("mean_period", &StrategyParams::mean_period)
        .def_readwrite("short_period", &StrategyParams::short_period)
        .def_readwrite("long_period", &StrategyParams::long_period)
        .def_readwrite("momentum_threshold", &StrategyParams::momentum_threshold)
        .def_readwrite("rsi_period", &StrategyParams::rsi_period)
        .def_readwrite("rsi_oversold", &StrategyParams::rsi_oversold)
        .def_readwrite("rsi_overbought", &StrategyParams::rsi_overbought)
        .def_readwrite("bb_period", &StrategyParams::bb_period)
        .def_readwrite("bb_std_dev", &StrategyParams::bb_std_dev)
        .def_readwrite("grid_spacing", &StrategyParams::grid_spacing)
        .def_readwrite("grid_levels", &StrategyParams::grid_levels);
    
    // 绑定 IStrategy 接口
    py::class_<IStrategy, std::shared_ptr<IStrategy>>(m, "Strategy")
        .def("process_market_data", &IStrategy::processMarketData)
        .def("initialize", &IStrategy::initialize)
        .def("cleanup", &IStrategy::cleanup)
        .def("get_status", &IStrategy::getStatus)
        .def("set_status", &IStrategy::setStatus)
        .def("set_params", &IStrategy::setParams)
        .def("get_params", &IStrategy::getParams);
    
    // 绑定 IStrategyEngine 接口
    py::class_<IStrategyEngine, std::shared_ptr<IStrategyEngine>>(m, "StrategyEngine")
        .def("initialize", &IStrategyEngine::initialize)
        .def("cleanup", &IStrategyEngine::cleanup)
        .def("set_strategy", &IStrategyEngine::setStrategy)
        .def("start", &IStrategyEngine::start)
        .def("stop", &IStrategyEngine::stop)
        .def("pause", &IStrategyEngine::pause)
        .def("get_status", &IStrategyEngine::getStatus)
        .def("process_market_data", &IStrategyEngine::processMarketData);
}

// 执行模块绑定
void bind_execution(py::module& m) {
    // 绑定 ExecutionStatus 枚举
    py::enum_<ExecutionStatus>(m, "ExecutionStatus")
        .value("IDLE", ExecutionStatus::IDLE)
        .value("CONNECTING", ExecutionStatus::CONNECTING)
        .value("CONNECTED", ExecutionStatus::CONNECTED)
        .value("DISCONNECTED", ExecutionStatus::DISCONNECTED)
        .value("ERROR", ExecutionStatus::ERROR);
    
    // 绑定 ExecutionResultStatus 枚举
    py::enum_<ExecutionResultStatus>(m, "ExecutionResultStatus")
        .value("SUCCESS", ExecutionResultStatus::SUCCESS)
        .value("FAILED", ExecutionResultStatus::FAILED)
        .value("PARTIAL", ExecutionResultStatus::PARTIAL);
    
    // 绑定 RiskParams 结构
    py::class_<RiskParams>(m, "RiskParams")
        .def(py::init<>())
        .def_readwrite("max_position_size", &RiskParams::max_position_size)
        .def_readwrite("max_daily_loss", &RiskParams::max_daily_loss)
        .def_readwrite("max_order_size", &RiskParams::max_order_size)
        .def_readwrite("max_orders_per_minute", &RiskParams::max_orders_per_minute);
    
    // 绑定 ExecutionResult 结构
    py::class_<ExecutionResult>(m, "ExecutionResult")
        .def(py::init<>())
        .def_readwrite("status", &ExecutionResult::status)
        .def_readwrite("order_id", &ExecutionResult::order_id)
        .def_readwrite("filled_quantity", &ExecutionResult::filled_quantity)
        .def_readwrite("average_price", &ExecutionResult::average_price)
        .def_readwrite("error_message", &ExecutionResult::error_message);
    
    // 绑定 IOrderExecutor 接口
    py::class_<IOrderExecutor, std::shared_ptr<IOrderExecutor>>(m, "OrderExecutor")
        .def("initialize", &IOrderExecutor::initialize)
        .def("cleanup", &IOrderExecutor::cleanup)
        .def("set_risk_params", &IOrderExecutor::setRiskParams)
        .def("set_api_credentials", &IOrderExecutor::setApiCredentials)
        .def("connect", &IOrderExecutor::connect)
        .def("disconnect", &IOrderExecutor::disconnect)
        .def("get_status", &IOrderExecutor::getStatus)
        .def("submit_order", &IOrderExecutor::submitOrder,
             py::arg("symbol"), py::arg("side"), py::arg("price"), py::arg("quantity"))
        .def("cancel_order", &IOrderExecutor::cancelOrder)
        .def("get_balance", &IOrderExecutor::getBalance)
        .def("get_position", &IOrderExecutor::getPosition)
        .def("get_order_status", &IOrderExecutor::getOrderStatus)
        .def("get_order_history", &IOrderExecutor::getOrderHistory,
             py::arg("max_count") = 100);
}

// 工厂类绑定
void bind_factory(py::module& m) {
    // Factory类只有静态方法，不需要实例化
    py::class_<CryptoQuantFactory>(m, "Factory")
        .def(py::init<>())  // 允许创建实例（虽然不需要）
        .def_static("create_strategy_engine", &CryptoQuantFactory::createStrategyEngine)
        .def_static("create_order_executor", &CryptoQuantFactory::createOrderExecutor)
        .def_static("create_orderbook_manager", &CryptoQuantFactory::createOrderbookManager)
        .def_static("create_market_data_fetcher", &CryptoQuantFactory::createMarketDataFetcher)
        .def_static("create_mean_reversion_strategy", &CryptoQuantFactory::createMeanReversionStrategy)
        .def_static("create_momentum_strategy", &CryptoQuantFactory::createMomentumStrategy)
        .def_static("create_rsi_strategy", &CryptoQuantFactory::createRSIStrategy);
    
    // 也可以直接在模块级别提供函数
    m.def("create_strategy_engine", &CryptoQuantFactory::createStrategyEngine);
    m.def("create_order_executor", &CryptoQuantFactory::createOrderExecutor);
    m.def("create_orderbook_manager", &CryptoQuantFactory::createOrderbookManager);
    m.def("create_market_data_fetcher", &CryptoQuantFactory::createMarketDataFetcher);
}

PYBIND11_MODULE(crypto_quant_python, m) {
    m.doc() = "Crypto Quant Trading System Python Bindings";
    
    // 版本信息
    m.attr("__version__") = CRYPTO_QUANT_VERSION_STRING;
    
    // 初始化函数
    m.def("init", &crypto_quant_init);
    m.def("cleanup", &crypto_quant_cleanup);
    m.def("get_version", &crypto_quant_get_version);
    m.def("get_version_string", &crypto_quant_get_version_string);
    
    // 日志功能
    m.def("log_debug", &crypto_quant_log_debug);
    m.def("log_info", &crypto_quant_log_info);
    m.def("log_warn", &crypto_quant_log_warn);
    m.def("log_error", &crypto_quant_log_error);
    m.def("log_critical", &crypto_quant_log_critical);
    m.def("init_logger", &crypto_quant_init_logger);
    
    // 绑定各个模块
    bind_market_data(m);
    bind_orderbook(m);
    bind_strategy(m);
    bind_execution(m);
    bind_factory(m);
}
