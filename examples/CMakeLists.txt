# 示例程序 CMakeLists.txt

# 简单 WebSocket 服务器
add_executable(simple_server simple_server.cc)
target_link_libraries(simple_server
    PRIVATE
        websocket_lib
        pthread
)
target_include_directories(simple_server
    PRIVATE
        ${PROJECT_INCLUDE_DIR}
)
target_compile_options(simple_server
    PRIVATE
        -Wall
        -Wextra
)
