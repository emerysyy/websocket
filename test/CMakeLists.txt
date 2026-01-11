# 测试 CMakeLists.txt

# 查找 GoogleTest
find_package(GTest REQUIRED)

# 创建测试可执行文件
add_executable(run_tests
    frame_parser_test.cc
    frame_builder_test.cc
    handshake_handler_test.cc
    jsonrpc_test.cc
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
