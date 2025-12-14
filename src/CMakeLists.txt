# 核心C/C++库
add_library(crypto_quant_core SHARED
    # 市场数据模块（C++实现）
    market_data/market_data_fetcher.cpp
    market_data/websocket_client.cpp
    
    # 订单薄模块（C++实现）
    orderbook/orderbook_manager.cpp
    
    # 策略引擎模块（C++实现）
    strategy/strategy_engine.cpp
    strategy/rsi_strategy.cpp
    strategy/momentum_strategy.cpp
    strategy/mean_reversion_strategy.cpp
    
    # 订单执行模块（C++实现）
    execution/order_executor.cpp
    
    # 工厂模块（C++实现）
    factory.cpp
    
    # 工具模块（C++实现）
    utils/logger.cpp
)

# 链接库
target_link_libraries(crypto_quant_core
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    pthread
)

# 链接nlohmann/json（如果通过包管理器安装）
if(nlohmann_json_FOUND)
    target_link_libraries(crypto_quant_core nlohmann_json::nlohmann_json)
endif()

# 链接spdlog和fmt（如果找到）
if(SPDLOG_LIBRARY)
    target_link_libraries(crypto_quant_core ${SPDLOG_LIBRARY})
endif()

if(FMT_LIBRARY)
    target_link_libraries(crypto_quant_core ${FMT_LIBRARY})
endif()

if(spdlog_FOUND)
    target_link_libraries(crypto_quant_core spdlog::spdlog)
endif()

if(fmt_FOUND)
    target_link_libraries(crypto_quant_core fmt::fmt)
endif()

# 设置输出目录 - 库文件输出到build/lib
set_target_properties(crypto_quant_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# 创建C++主程序可执行文件
add_executable(crypto_quant_main
    main.cpp
)

target_link_libraries(crypto_quant_main
    crypto_quant_core
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    pthread
)

# 链接spdlog和fmt（如果找到）
if(SPDLOG_LIBRARY)
    target_link_libraries(crypto_quant_main ${SPDLOG_LIBRARY})
endif()

if(FMT_LIBRARY)
    target_link_libraries(crypto_quant_main ${FMT_LIBRARY})
endif()

if(spdlog_FOUND)
    target_link_libraries(crypto_quant_main spdlog::spdlog)
endif()

if(fmt_FOUND)
    target_link_libraries(crypto_quant_main fmt::fmt)
endif()

# 设置release目录路径（相对于项目根目录）
set(RELEASE_DIR ${CMAKE_SOURCE_DIR}/release)

# 设置可执行文件的输出目录为release
set_target_properties(crypto_quant_main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${RELEASE_DIR}
)

# 复制配置文件到release目录（在构建后执行）
add_custom_command(
    TARGET crypto_quant_main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.json
        ${RELEASE_DIR}/config.json
    COMMENT "Copying config.json to release directory"
)

