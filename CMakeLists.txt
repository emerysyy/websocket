# 设置 CMake 最低版本要求为 3.20
cmake_minimum_required(VERSION 3.20)

# 项目名称
project(websocket)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 版本号设置
set(MyLib_MAJOR_VERSION 1)
set(MyLib_MINOR_VERSION 0)
set(MyLib_PATCH_VERSION 0)

# 设置全局头文件搜索路径
set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_INCLUDE_DIR})

# 第三方库路径
set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libs")

# darwincore 库路径
set(DARWIN_CORE_DIR "${LIBS_DIR}/darwincore")
set(JSON_DIR "${LIBS_DIR}/nlohmann")

message("include:${DARWIN_CORE_DIR}/include")

# 包含 darwincore 头文件
include_directories(
    "${DARWIN_CORE_DIR}/include"
    "${JSON_DIR}/include"
)

# 设置 darwincore 库路径
link_directories("${DARWIN_CORE_DIR}/lib")

# 添加 src 子目录
add_subdirectory(src)

# 添加测试子目录
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

# 添加示例子目录
add_subdirectory(examples)
