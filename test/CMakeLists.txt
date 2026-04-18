# 测试 CMakeLists.txt

# 查找 GoogleTest
find_package(GTest REQUIRED)

# 创建测试可执行文件
add_executable(run_tests
    websocket/frame_parser_test.cc
    websocket/frame_builder_test.cc
    websocket/handshake_handler_test.cc
    websocket/websocket_session_test.cc
    websocket/websocket_server_test.cc
    jsonrpc/request_handler_test.cc
    jsonrpc/notification_builder_test.cc
    jsonrpc/jsonrpc_server_test.cc
)

target_link_libraries(run_tests
    PRIVATE
        websocket_lib
        GTest::GTest
        GTest::Main
        pthread
)

target_include_directories(run_tests
    PRIVATE
        ${PROJECT_INCLUDE_DIR}
)

target_compile_options(run_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

include(GoogleTest)
gtest_discover_tests(run_tests)
