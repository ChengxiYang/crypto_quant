#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <cstring>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include "crypto_quant.h"

// C++11风格的日志系统
class CryptoQuantLogger {
private:
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<bool> initialized_;
    std::mutex init_mutex_;
    
public:
    CryptoQuantLogger() : initialized_(false) {}
    
    void init() {
        if (initialized_.load()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(init_mutex_);
        if (initialized_.load()) {
            return;
        }
        
        try {
            // 创建控制台输出sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%P] %v");
            
            // 创建文件输出sink（输出到logs目录）
            std::string log_path = "logs/crypto_quant.log";
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_path, 1024 * 1024 * 5, 3);
            file_sink->set_level(spdlog::level::debug);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%P] %v");
            
            // 创建日志记录器
            std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};
            logger_ = std::make_shared<spdlog::logger>("crypto_quant", sinks.begin(), sinks.end());
            logger_->set_level(spdlog::level::debug);
            
            // 注册为默认日志记录器
            spdlog::register_logger(logger_);
            spdlog::set_default_logger(logger_);
            spdlog::flush_on(spdlog::level::info);
            
            initialized_.store(true);
            
        } catch (const spdlog::spdlog_ex& ex) {
            // 如果文件日志失败，只使用控制台日志
            logger_ = spdlog::stdout_color_mt("crypto_quant");
            logger_->set_level(spdlog::level::info);
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%P] %v");
            spdlog::set_default_logger(logger_);
            initialized_.store(true);
        }
    }
    
    void log(const char* level, const char* message) {
        if (!initialized_.load()) {
            init();
        }
        
        if (!logger_) {
            return;
        }
        
        std::string level_str(level);
        if (level_str == "debug") {
            logger_->debug(message);
        } else if (level_str == "info") {
            logger_->info(message);
        } else if (level_str == "warn") {
            logger_->warn(message);
        } else if (level_str == "error") {
            logger_->error(message);
        } else if (level_str == "critical") {
            logger_->critical(message);
        } else {
            logger_->info(message);
        }
    }
    
    void flush() {
        if (logger_) {
            logger_->flush();
        }
    }
};

static CryptoQuantLogger g_logger;

// 初始化日志系统
void crypto_quant_init_logger(void) {
    g_logger.init();
}

// 高性能日志实现
void crypto_quant_log_message(const char* level, const char* message) {
    g_logger.log(level, message);
}

// 便捷的日志函数
void crypto_quant_log_debug(const char* message) {
    g_logger.log("debug", message);
}

void crypto_quant_log_info(const char* message) {
    g_logger.log("info", message);
}

void crypto_quant_log_warn(const char* message) {
    g_logger.log("warn", message);
}

void crypto_quant_log_error(const char* message) {
    g_logger.log("error", message);
}

void crypto_quant_log_critical(const char* message) {
    g_logger.log("critical", message);
}

// 获取版本信息
const char* crypto_quant_get_version(void) {
    return CRYPTO_QUANT_VERSION_STRING;
}

const char* crypto_quant_get_version_string(void) {
    return CRYPTO_QUANT_VERSION_STRING;
}

// 初始化库
int crypto_quant_init(void) {
    crypto_quant_init_logger();
    crypto_quant_log_info("Crypto Quant v1.0.0");
    crypto_quant_log_info("Crypto Quant System initialized");
    return 0;
}

// 清理库
void crypto_quant_cleanup(void) {
    g_logger.flush();
    crypto_quant_log_info("Crypto Quant System cleanup completed");
}