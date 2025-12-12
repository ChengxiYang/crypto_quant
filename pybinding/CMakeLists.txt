# Python绑定
find_package(pybind11 REQUIRED)

# Python扩展模块
pybind11_add_module(crypto_quant_python
    python_bindings.cpp
)

# 链接核心库和依赖
target_link_libraries(crypto_quant_python PRIVATE 
    crypto_quant_core
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# 如果找到spdlog和fmt，链接它们
if(SPDLOG_INCLUDE_DIR AND SPDLOG_LIBRARY)
    target_include_directories(crypto_quant_python PRIVATE ${SPDLOG_INCLUDE_DIR})
    target_link_libraries(crypto_quant_python PRIVATE ${SPDLOG_LIBRARY})
endif()

if(fmt_FOUND)
    target_link_libraries(crypto_quant_python PRIVATE fmt::fmt)
endif()

# 设置输出目录
set_target_properties(crypto_quant_python PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python
)

# 设置C++11标准
set_target_properties(crypto_quant_python PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)
