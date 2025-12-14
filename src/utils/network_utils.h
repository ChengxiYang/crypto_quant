#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <stdint.h>
#include <arpa/inet.h>

// 检测字节序（编译时检测）
#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define IS_LITTLE_ENDIAN 1
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define IS_LITTLE_ENDIAN 0
    #else
        #define IS_LITTLE_ENDIAN 1  // 默认假设小端
    #endif
#elif defined(__LITTLE_ENDIAN__) || defined(_WIN32) || defined(__i386__) || defined(__x86_64__)
    #define IS_LITTLE_ENDIAN 1
#elif defined(__BIG_ENDIAN__)
    #define IS_LITTLE_ENDIAN 0
#else
    // 默认假设小端（大多数现代系统都是小端）
    #define IS_LITTLE_ENDIAN 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 字节序转换工具函数
static inline uint16_t hton16(uint16_t host) {
    return htons(host);
}

static inline uint32_t hton32(uint32_t host) {
    return htonl(host);
}

static inline uint64_t hton64(uint64_t host) {
#if IS_LITTLE_ENDIAN
    return ((uint64_t)htonl((uint32_t)(host >> 32))) | 
           ((uint64_t)htonl((uint32_t)host) << 32);
#else
    return host;
#endif
}

static inline uint16_t ntoh16(uint16_t net) {
    return ntohs(net);
}

static inline uint32_t ntoh32(uint32_t net) {
    return ntohl(net);
}

static inline uint64_t ntoh64(uint64_t net) {
#if IS_LITTLE_ENDIAN
    return ((uint64_t)ntohl((uint32_t)(net >> 32))) | 
           ((uint64_t)ntohl((uint32_t)net) << 32);
#else
    return net;
#endif
}

// 双精度浮点数网络字节序转换（通过联合体实现）
static inline double hton_double(double host) {
    union {
        double d;
        uint64_t i;
    } u;
    u.d = host;
    u.i = hton64(u.i);
    return u.d;
}

static inline double ntoh_double(double net) {
    union {
        double d;
        uint64_t i;
    } u;
    u.d = net;
    u.i = ntoh64(u.i);
    return u.d;
}

#ifdef __cplusplus
}
#endif

#endif // NETWORK_UTILS_H
