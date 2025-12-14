#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <nlohmann/json.hpp>
#include "crypto_quant.h"

using json = nlohmann::json;

using namespace crypto_quant;

// 全局变量用于控制程序退出
static std::atomic<bool> g_running(true);
static std::atomic<int> g_market_data_count(0);

// 信号处理函数
void signal_handler(int sig) {
    std::cout << "\n收到信号 " << sig << "，正在退出...\n";
    g_running = false;
}

// 将symbol_t转换为字符串
std::string symbol_to_string(symbol_t symbol) {
    switch (symbol) {
        case SYMBOL_BTC_USDT: return "BTC/USDT";
        case SYMBOL_ETH_USDT: return "ETH/USDT";
        case SYMBOL_BTC_ETH: return "BTC/ETH";
        default: return "UNKNOWN";
    }
}

// 市场数据回调函数
void on_market_data(const orderbook_t& orderbook) {
    g_market_data_count++;
    
    double best_bid = 0.0;
    double best_ask = 0.0;
    
    if (orderbook.bid_count > 0) {
        best_bid = orderbook.bids[0].price;
    }
    if (orderbook.ask_count > 0) {
        best_ask = orderbook.asks[0].price;
    }
    
    double mid_price = (best_bid > 0 && best_ask > 0) ? (best_bid + best_ask) / 2.0 : 0.0;
    double spread = (best_bid > 0 && best_ask > 0) ? (best_ask - best_bid) : 0.0;
    
    // 格式化时间戳
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(orderbook.timestamp)
    );
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    std::tm* tm = std::localtime(&time_t);
    
    std::cout << "\r[" << std::put_time(tm, "%H:%M:%S") << "] "
              << symbol_to_string(orderbook.symbol) << " | "
              << "买: " << std::fixed << std::setprecision(2) << best_bid << " | "
              << "卖: " << best_ask << " | "
              << "中间: " << mid_price << " | "
              << "价差: " << spread
              << std::flush;
}

// 打印使用说明
void print_usage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项]\n";
    std::cout << "\n选项:\n";
    std::cout << "  --symbol SYMBOL      交易对 (BTC_USDT, ETH_USDT, BTC_ETH) [默认: 从config.json读取]\n";
    std::cout << "  --api-key KEY       币安API密钥 [默认: 从config.json读取]\n";
    std::cout << "  --api-secret SECRET 币安API密钥 [默认: 从config.json读取]\n";
    std::cout << "  --config FILE        配置文件路径 [默认: config.json]\n";
    std::cout << "  --test-order         测试下单（需要API密钥）\n";
    std::cout << "  --help               显示此帮助信息\n";
    std::cout << "\n环境变量:\n";
    std::cout << "  BINANCE_API_KEY      币安API密钥\n";
    std::cout << "  BINANCE_API_SECRET   币安API密钥\n";
    std::cout << "\n示例:\n";
    std::cout << "  " << program_name << " --symbol BTC_USDT\n";
    std::cout << "  " << program_name << " --symbol ETH_USDT --api-key YOUR_KEY --api-secret YOUR_SECRET\n";
    std::cout << "  " << program_name << " --symbol BTC_USDT --test-order\n";
}

// 配置结构
struct Config {
    symbol_t symbol = SYMBOL_BTC_USDT;
    std::string api_key;
    std::string api_secret;
    bool test_order = false;
    bool testnet = false;
    double max_order_size = 1000.0;
    double max_daily_loss = 100.0;
    int max_orders_per_minute = 10;
    bool enable_risk_control = true;
    std::string config_file = "config.json";
};

// 从字符串转换为symbol_t
symbol_t string_to_symbol(const std::string& symbol_str) {
    if (symbol_str == "BTCUSDT" || symbol_str == "BTC_USDT") {
        return SYMBOL_BTC_USDT;
    } else if (symbol_str == "ETHUSDT" || symbol_str == "ETH_USDT") {
        return SYMBOL_ETH_USDT;
    } else if (symbol_str == "BTCETH" || symbol_str == "BTC_ETH") {
        return SYMBOL_BTC_ETH;
    }
    return SYMBOL_BTC_USDT;
}

// 从配置文件加载配置
bool load_config_from_file(Config& config, const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "警告: 无法打开配置文件 " << config_file 
                  << "，使用默认配置\n";
        return false;
    }
    
    try {
        json j;
        file >> j;
        
        // 读取execution配置
        if (j.contains("execution")) {
            const auto& exec = j["execution"];
            if (exec.contains("api_key") && !exec["api_key"].is_null()) {
                config.api_key = exec["api_key"].get<std::string>();
            }
            if (exec.contains("secret_key") && !exec["secret_key"].is_null()) {
                config.api_secret = exec["secret_key"].get<std::string>();
            }
            if (exec.contains("test_order")) {
                config.test_order = exec["test_order"].get<bool>();
            }
            if (exec.contains("testnet")) {
                config.testnet = exec["testnet"].get<bool>();
            }
            if (exec.contains("max_order_size")) {
                config.max_order_size = exec["max_order_size"].get<double>();
            }
            if (exec.contains("max_daily_loss")) {
                config.max_daily_loss = exec["max_daily_loss"].get<double>();
            }
            if (exec.contains("max_orders_per_second")) {
                config.max_orders_per_minute = exec["max_orders_per_second"].get<int>() * 60;
            }
            if (exec.contains("enable_risk_control")) {
                config.enable_risk_control = exec["enable_risk_control"].get<bool>();
            }
        }
        
        // 读取market_data配置中的symbols
        if (j.contains("market_data")) {
            const auto& market_data = j["market_data"];
            if (market_data.contains("symbols") && market_data["symbols"].is_array()) {
                const auto& symbols = market_data["symbols"];
                if (!symbols.empty()) {
                    std::string first_symbol = symbols[0].get<std::string>();
                    config.symbol = string_to_symbol(first_symbol);
                }
            }
        }
        
        std::cout << "成功加载配置文件: " << config_file << "\n";
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "错误: JSON解析失败: " << e.what() << "\n";
        return false;
    } catch (const json::type_error& e) {
        std::cerr << "错误: JSON类型错误: " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "错误: 读取配置文件失败: " << e.what() << "\n";
        return false;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "加密货币量化交易系统 - C++主程序\n";
    std::cout << "========================================\n";
    std::cout << "版本: " << crypto_quant_get_version_string() << "\n\n";
    
    // 解析配置文件
    Config config;
    load_config_from_file(config, "config.json");
    
    // 初始化库
    if (crypto_quant_init() != 0) {
        std::cerr << "错误: 无法初始化库\n";
        return 1;
    }
    
    crypto_quant_init_logger();
    crypto_quant_log_info("C++主程序启动");
    
    // 注册信号处理器
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        // 创建组件
        auto market_data_fetcher = CryptoQuantFactory::createMarketDataFetcher();
        auto order_executor = CryptoQuantFactory::createOrderExecutor();
        auto orderbook_manager = CryptoQuantFactory::createOrderbookManager();
        
        if (!market_data_fetcher || !order_executor || !orderbook_manager) {
            crypto_quant_log_error("创建组件失败");
            return 1;
        }
        
        // 初始化组件
        if (!market_data_fetcher->initialize()) {
            crypto_quant_log_error("市场数据提供者初始化失败");
            return 1;
        }
        
        if (!order_executor->initialize()) {
            crypto_quant_log_error("订单执行器初始化失败");
            return 1;
        }
        
        if (!orderbook_manager->initialize()) {
            crypto_quant_log_error("订单薄管理器初始化失败");
            return 1;
        }
        
        crypto_quant_log_info("所有组件初始化成功");
        
        // 设置市场数据回调
        market_data_fetcher->setOrderbookCallback([&orderbook_manager](const orderbook_t& orderbook) {
            // 更新订单薄管理器
            orderbook_manager->updateOrderbook(orderbook);
            
            // 显示市场数据
            on_market_data(orderbook);
        });
        
        // 设置币安数据源
        market_data_fetcher->setDataSources(true, false);  // 只使用币安
        
        // 启动市场数据收集
        std::cout << "\n启动市场数据收集 (" << symbol_to_string(config.symbol) << ")...\n";
        if (!market_data_fetcher->start(config.symbol)) {
            crypto_quant_log_error("启动市场数据收集失败");
            return 1;
        }
        
        // 如果提供了API密钥，连接订单执行器
        if (!config.api_key.empty() && !config.api_secret.empty()) {
            std::cout << "\n连接币安交易所...\n";
            
            // 设置风险参数（从配置文件读取）
            RiskParams risk_params;
            risk_params.max_position_size = config.max_order_size * 10.0;  // 最大持仓为最大订单的10倍
            risk_params.max_daily_loss = config.max_daily_loss;
            risk_params.max_order_size = config.max_order_size;
            risk_params.max_orders_per_minute = config.max_orders_per_minute;
            order_executor->setRiskParams(risk_params);
            
            std::cout << "风险参数: 最大订单=" << config.max_order_size 
                      << ", 最大日亏损=" << config.max_daily_loss
                      << ", 每分钟最大订单数=" << config.max_orders_per_minute << "\n";
            
            // 设置API凭据
            order_executor->setApiCredentials(config.api_key, config.api_secret);
            
            // 连接
            if (order_executor->connect()) {
                std::cout << "连接币安交易所成功\n";
                
                // 查询余额
                double balance = order_executor->getBalance(config.symbol);
                std::cout << "账户余额: " << std::fixed << std::setprecision(8) << balance << "\n";
                
                // 如果启用了测试下单
                if (config.test_order) {
                    std::cout << "\n测试下单功能...\n";
                    std::cin.get();
                    
                    // 获取当前价格
                    orderbook_t current_orderbook = orderbook_manager->getOrderbook(config.symbol);
                    if (current_orderbook.bid_count > 0 && current_orderbook.ask_count > 0) {
                        double best_bid = current_orderbook.bids[0].price;
                        double best_ask = current_orderbook.asks[0].price;
                        double mid_price = (best_bid + best_ask) / 2.0;
                        
                        std::cout << "当前价格: 买=" << best_bid << ", 卖=" << best_ask 
                                  << ", 中间=" << mid_price << "\n";
                        
                        // 提交一个限价买单（价格低于当前买价，不会立即成交）
                        double order_price = best_bid * 0.95;  // 低于当前买价5%
                        double order_quantity = 0.0001;  // 非常小的数量
                        
                        std::cout << "提交限价买单: 价格=" << order_price 
                                  << ", 数量=" << order_quantity << "\n";
                        
                        ExecutionResult result = order_executor->submitOrder(
                            config.symbol,
                            0,  // BUY
                            order_price,
                            order_quantity
                        );
                        
                        if (result.status == ExecutionResultStatus::SUCCESS) {
                            std::cout << "✅ 订单提交成功，订单ID: " << result.order_id << "\n";
                            
                            // 等待一下
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            
                            // 查询订单状态
                            ExecutionResult status = order_executor->getOrderStatus(result.order_id);
                            std::cout << "订单状态: ";
                            switch (status.status) {
                                case ExecutionResultStatus::SUCCESS:
                                    std::cout << "已成交\n";
                                    break;
                                case ExecutionResultStatus::PARTIAL:
                                    std::cout << "部分成交\n";
                                    break;
                                case ExecutionResultStatus::FAILED:
                                    std::cout << "失败: " << status.error_message << "\n";
                                    break;
                            }
                            
                            // 撤销订单（如果未成交）
                            if (status.status != ExecutionResultStatus::SUCCESS) {
                                std::cout << "撤销订单...\n";
                                if (order_executor->cancelOrder(result.order_id)) {
                                    std::cout << "订单撤销成功\n";
                                } else {
                                    std::cout << "订单撤销失败\n";
                                }
                            }
                        } else {
                            std::cout << "订单提交失败: " << result.error_message << "\n";
                        }
                    } else {
                        std::cout << "无法获取当前价格，跳过下单测试\n";
                    }
                }
            } else {
                std::cout << "连接币安交易所失败\n";
                std::cout << "提示: 请检查API密钥是否正确\n";
            }
        } else {
            std::cout << "\n提示: 未设置API密钥，仅显示市场数据\n";
            std::cout << "      设置环境变量 BINANCE_API_KEY 和 BINANCE_API_SECRET 以启用交易功能\n";
        }
        
        // 主循环
        std::cout << "\n市场数据实时更新中... (按 Ctrl+C 退出)\n";
        std::cout << "已接收数据: 0 条\n";
        
        auto last_stats_time = std::chrono::steady_clock::now();
        
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 每秒更新一次统计信息
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 1) {
                int count = g_market_data_count.load();
                std::cout << "\n已接收数据: " << count << " 条" << std::flush;
                last_stats_time = now;
            }
        }
        
        // 停止组件
        std::cout << "\n\n正在停止...\n";
        market_data_fetcher->stop();
        if (order_executor->getStatus() == ExecutionStatus::CONNECTED) {
            order_executor->disconnect();
        }
        
        // 清理组件
        // market_data_fetcher->cleanup();
        order_executor->cleanup();
        orderbook_manager->cleanup();
        
        crypto_quant_log_info("所有组件已清理");
        
    } catch (const std::exception& e) {
        crypto_quant_log_error(("异常: " + std::string(e.what())).c_str());
        return 1;
    }
    
    // 清理库
    crypto_quant_cleanup();
    
    std::cout << "\n程序退出\n";
    return 0;
}

