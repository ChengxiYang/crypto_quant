cmake_minimum_required(VERSION 3.10)
project(crypto_quant VERSION 1.0.0 LANGUAGES C CXX)

# 设置C/C++标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（如果没有指定，默认为Release）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# 根据构建类型设置编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug模式：使用-g，优化级别为0
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -O0")
    message(STATUS "构建类型: Debug (使用 -g -O0)")
else()
    # Release模式：使用-O3优化
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -O3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -O3")
    message(STATUS "构建类型: ${CMAKE_BUILD_TYPE} (使用 -O3)")
endif()

# 启用C++11特性
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_C99")

# 查找依赖包
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
# 查找spdlog
find_package(spdlog QUIET)
# 查找fmt（spdlog的依赖）
find_package(fmt QUIET)
# 查找OpenSSL
find_package(OpenSSL REQUIRED)
# 查找libcurl
pkg_check_modules(CURL REQUIRED libcurl)
# nlohmann/json是单头文件库，不需要查找包

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party  # nlohmann/json.hpp
    ${CURL_INCLUDE_DIRS}
)

# 查找nlohmann/json（如果通过包管理器安装）
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "Found nlohmann/json via find_package")
else()
    message(STATUS "Using nlohmann/json from third_party directory")
endif()

if(SPDLOG_INCLUDE_DIR)
    include_directories(${SPDLOG_INCLUDE_DIR})
endif()

if(fmt_FOUND)
    include_directories(${fmt_INCLUDE_DIRS})
endif()

# 添加子目录
add_subdirectory(src)

# Python绑定（可选）
# 注意：python_bindings.cpp用于构建C++扩展模块
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    message(STATUS "Found pybind11, enabling Python bindings")
    if(EXISTS ${CMAKE_SOURCE_DIR}/pybinding)
        add_subdirectory(pybinding)
    else()
        message(WARNING "pybinding directory not found, skipping Python bindings")
    endif()
else()
    message(STATUS "pybind11 not found, Python bindings disabled")
endif()

# 创建release目录
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/release)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/release/logs)

# 安装规则
install(TARGETS crypto_quant_main
    RUNTIME DESTINATION bin
)

install(TARGETS crypto_quant_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)