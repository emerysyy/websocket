# CMakeLists.txt 修正

DarwinCore 库和头文件位于 `3rd/` 目录：

```
3rd/
├── include/darwincore/    # 头文件
└── lib/                   # 静态库和动态库
```

---

## 修改内容

```cmake
# 旧路径（已失效）
# set(DARWIN_CORE_DIR "${LIBS_DIR}/darwincore")
# include_directories("${DARWIN_CORE_DIR}/include")
# link_directories("${DARWIN_CORE_DIR}/lib")

# 新路径
set(DARWIN_CORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rd")
include_directories("${DARWIN_CORE_ROOT}/include")
link_directories("${DARWIN_CORE_ROOT}/lib")

# 链接时添加 darwincore 库
target_link_libraries(your_target
    darwincore
    # ... 其他库
)
```

---

## 依赖处理

如果 DarwinCore 有依赖（如 pthread），也需要一并链接：

```cmake
find_package(Threads REQUIRED)
target_link_libraries(your_target
    darwincore
    Threads::Threads
)
```

---

## 完整示例

```cmake
cmake_minimum_required(VERSION 3.14)
project(WebSocketServer)

set(CMAKE_CXX_STANDARD 17)

# DarwinCore
set(DARWIN_CORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rd")
include_directories("${DARWIN_CORE_ROOT}/include")
link_directories("${DARWIN_CORE_ROOT}/lib")

# 查找依赖
find_package(Threads REQUIRED)

# 源文件
set(SOURCES
    src/frame_builder.cc
    src/frame_parser.cc
    src/handshake_handler.cc
    src/jsonrpc/notification_builder.cc
    src/jsonrpc/request_handler.cc
    src/jsonrpc_server.cc
)

# 可执行文件
add_executable(websocket_server ${SOURCES})

# 链接库
target_link_libraries(websocket_server
    darwincore
    Threads::Threads
)
```
