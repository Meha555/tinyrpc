cmake_minimum_required(VERSION 3.15)
project(tinyrpc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(CMAKE_EXPORT_COMPILE_COMMANDS "for LSP" ON)

set(BUILD_EXAMPLE ON)

#查找porotbuf包
find_package(Protobuf REQUIRED)

# 查找 protoc 编译器
find_program(PROTOC_EXECUTABLE NAMES protoc)
if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc compiler not found")
endif()

#添加子目录
add_subdirectory(src)
if (BUILD_EXAMPLE)
    add_subdirectory(example)
endif()