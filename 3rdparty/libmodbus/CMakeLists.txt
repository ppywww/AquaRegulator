cmake_minimum_required(VERSION 3.10)

project(modbus LANGUAGES C)

# 将所有源码文件列出来
set(MODBUS_SOURCES
    modbus.c
    modbus-rtu.c
    modbus-tcp.c
    modbus-data.c
)

# 创建库目标
add_library(modbus SHARED ${MODBUS_SOURCES})

target_include_directories(modbus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # 让外部 target_link_libraries 时能找到 modbus.h
)

if(UNIX)
    target_link_libraries(modbus PRIVATE m pthread)
elseif(WIN32)
    target_compile_definitions(modbus PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()