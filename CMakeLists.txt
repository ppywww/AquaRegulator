cmake_minimum_required(VERSION 3.16)
project(AquaRegulator VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if (WIN32)
    set(HPSOCKET_ROOT "${CMAKE_SOURCE_DIR}/../3rdparty/win32/hp-socket-6.0.7-lib-win")
elseif(UNIX)
    set(HPSOCKET_ROOT "${CMAKE_SOURCE_DIR}/../3rdparty/linux/hp-socket-6.0.7-lib-linux")
endif()

add_subdirectory(3rdparty/libmodbus)

message(STATUS "HPSOCKET_ROOT: ${HPSOCKET_ROOT}")

include_directories(${HPSOCKET_ROOT}/include/hpsocket)
link_directories(${HPSOCKET_ROOT}/lib/hpsocket/x64)

add_executable(AquaRegS 
    main.cxx
    services/transport/video_manager.cxx
    core/logger.cxx
    core/configuration.cxx
    infrastructure/database/mariadb_client.cxx
    infrastructure/database/telemetry_repository.cxx
    monitoring/health_monitor.cxx
)

target_include_directories(AquaRegS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(AquaRegS PRIVATE hpsocket modbus mariadbclient Threads::Threads)

if (WIN32)
    add_custom_command(TARGET AquaRegS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${HPSOCKET_ROOT}/lib/hpsocket/x64/HPSocket.dll"
    $<TARGET_FILE_DIR:AquaRegS>
    )
elseif(UNIX)
    add_custom_command(
        TARGET AquaRegS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "hello from post build script!"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HPSOCKET_ROOT}/lib/hpsocket/x64/libhpsocket.so"
            "${CMAKE_BINARY_DIR}/libhpsocket.so"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "libhpsocket.so"
            "${CMAKE_BINARY_DIR}/libhpsocket.so.6"
        COMMENT "Copying libhpsocket.so and creating symlink"
    )
endif()
