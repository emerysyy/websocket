# 源文件列表
set(SOURCES
    frame_parser.cc
    frame_builder.cc
    handshake_handler.cc
    jsonrpc_server.cc
    jsonrpc/request_handler.cc
    jsonrpc/notification_builder.cc
)

# 创建静态库
add_library(websocket_lib STATIC ${SOURCES})

# 设置库输出目录
set_target_properties(websocket_lib PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib
)

# 链接依赖
target_link_libraries(websocket_lib
    PUBLIC
        darwincore_network
        pthread
)

# 设置包含路径
target_include_directories(websocket_lib
    PUBLIC
        ${PROJECT_INCLUDE_DIR}
)

# 编译选项
target_compile_options(websocket_lib
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
