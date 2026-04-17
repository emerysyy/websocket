# WebSocket 示例

# Echo 服务器
add_executable(echo_server echo_server.cc)
target_link_libraries(echo_server
    PRIVATE
        websocket_lib
        pthread
)
target_include_directories(echo_server
    PRIVATE
        ${PROJECT_INCLUDE_DIR}
)
target_compile_options(echo_server
    PRIVATE
        -Wall
        -Wextra
)
