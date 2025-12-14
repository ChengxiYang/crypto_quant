#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <memory>

#include "crypto_quant.h"

namespace crypto_quant {

// WebSocket 客户端类
class WebSocketClient {
private:
    std::string url_;
    void* curl_;  // CURL* 类型，使用 void* 避免在头文件中暴露 curl 头文件
    std::thread worker_thread_;
    std::atomic<bool> is_running_;
    std::function<void(const orderbook_t*)> callback_;
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_;

    // 禁止拷贝和赋值
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    // 内部方法
    size_t onDataReceived(char* data, size_t size);
    orderbook_t parseOrderbook(const void* json_obj, const std::string& stream_name) const;  // json_obj 是 json* 类型
    void workerThread();

    // CURL 回调函数
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

public:
    explicit WebSocketClient(const std::string& url);
    ~WebSocketClient();

    void setCallback(std::function<void(const orderbook_t*)> callback);
    bool start();
    bool stop();
    bool isRunning() const;
    bool isInitialized() const;
};

} // namespace crypto_quant

#endif // WEBSOCKET_CLIENT_H

