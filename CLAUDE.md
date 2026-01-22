# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 **DarwinCore Network** 库构建的 **WebSocket + JSON-RPC 2.0 服务器**，使用 C++17 实现。它通过 WebSocket 协议（RFC 6455）与 JavaScript 客户端进行双向通信，并使用 JSON-RPC 2.0 进行方法调用。

## 构建和测试命令

### 基本构建

```bash
# 从项目根目录
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### 运行测试

```bash
cd build
make
./test/run_tests
```

### 运行单个测试

```bash
cd build
./test/run_tests --gtest_filter=FrameParserTest.ParseSimpleTextFrame
```

### 运行示例服务器

```bash
cd build
./examples/simple_server
# 服务器将在 ws://127.0.0.1:9998 监听
```

### 清理重建

```bash
cd build
make clean
rm -rf *
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## 架构设计

代码采用分层架构，从下到上依次为：

### 1. WebSocket 协议层 (`src/frame_*.cc`, `src/handshake_handler.cc`)

- **FrameParser**: 解析传入的 WebSocket 帧，处理掩码、扩展长度
- **FrameBuilder**: 构建传出的 WebSocket 帧
- **HandshakeHandler**: 处理 WebSocket HTTP 握手，计算 Sec-WebSocket-Accept

### 2. JSON-RPC 层 (`src/jsonrpc/`)

- **RequestHandler**: 路由和处理 JSON-RPC 2.0 请求，支持批量请求
- **NotificationBuilder**: 构建服务器主动推送的通知消息

### 3. 应用层 (`src/jsonrpc_server.cc`)

- **JsonRpcServer**: 整合 WebSocket 和 JSON-RPC，管理所有连接状态

### 4. 传输层 (DarwinCore Network 外部库)

- **Acceptor-Reactor-Worker 线程模型**
- **事件驱动 I/O** (kqueue/epoll)

## 线程安全关键点

**最重要**: DarwinCore Network 的回调在 Worker 线程中执行。处理这些回调时：

- **必须最小化锁的作用域**，避免死锁
- **绝不在持有 `connections_mutex_` 时调用用户回调**
- 用户回调可能会回调服务器方法（例如 `GetConnectionCount()`）

正确模式（参考 `jsonrpc_server.cc:141`）：

```cpp
void OnNetworkConnected(const ConnectionInformation& info) {
    ConnectionState state;
    state.parser = std::make_unique<FrameParser>();

    // ✅ 所有加锁操作在一个作用域内
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[info.connection_id] = std::move(state);
    }  // 锁在此处释放

    // ✅ 用户回调在锁外调用
    if (on_connected_) {
        on_connected_(info.connection_id, info);
    }
}
```

如果违反此模式会导致死锁：用户回调 → `GetConnectionCount()` → 再次获取 `connections_mutex_` → 死锁。

## 依赖库位置

项目依赖 `../libs/` 目录下的外部库：

1. **DarwinCore Network** (`../libs/darwincore/`)
   - 头文件: `include/darwincore/`
   - 库文件: `lib/libdarwincore_network.a` (或 `.dylib`)
   - 提供: `darwincore::network::Server`、事件回调、连接管理

2. **nlohmann/json** (`../libs/nlohmann/`)
   - 头文件库，位于 `include/`
   - 用于 JSON-RPC 消息解析和生成

## 数据流：从接收到响应

完整请求流程（以 JSON-RPC 调用为例）：

```
客户端发送 WebSocket 帧 (已掩码)
    ↓
DarwinCore Reactor 线程读取数据
    ↓
Worker 线程触发 kData 事件
    ↓
JsonRpcServer::OnNetworkMessage()
    ├─ 如果未握手 → HandleHandshake()
    └─ 已握手 → 追加到 recv_buffer
              ↓
         循环调用 FrameParser::Parse()
              ↓
         解除掩码 (XOR 算法)
              ↓
         HandleWebSocketFrame()
              ↓
         HandleJsonRpcRequest()
              ↓
         RequestHandler::HandleRequest()
              ↓
         查找并调用注册的方法
              ↓
         FrameBuilder::CreateTextFrame()
              ↓
         network_server_->SendData()
              ↓
客户端收到响应
```

## WebSocket 帧结构（RFC 6455）

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+---------------------------------------------------------------+
```

- **FIN**: 1 = 最后一帧，0 = 后续还有帧
- **Opcode**: 0x1 (文本)，0x2 (二进制)，0x8 (关闭)，0x9 (Ping)，0xA (Pong)
- **MASK**: 客户端帧必须为 1，服务器帧必须为 0
- **Payload length**: 7 位，或 16 位（如果 126），或 64 位（如果 127）

## 连接状态管理

每个连接在 `JsonRpcServer` 中维护：

1. **handshake_completed**: WebSocket 握手是否完成
2. **recv_buffer**: 接收缓冲区（累积数据，可能包含不完整帧）
3. **parser**: 该连接专用的 FrameParser 实例

消息处理循环：
1. 收到数据 → 追加到 `recv_buffer`
2. 尝试解析帧 → 如果不完整，等待更多数据
3. 如果完整帧 → 从缓冲区移除并处理
4. 重复直到没有更多完整帧

参考实现：`jsonrpc_server.cc:180` (`OnNetworkMessage`)

## 关键实现细节

### WebSocket 握手

握手处理器验证 HTTP 升级请求并计算 Sec-WebSocket-Accept 头：

```cpp
accept_key = base64(sha1(websocket_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
```

实现位置：`handshake_handler.cc:112`

### 帧掩码（客户端 → 服务器）

客户端**必须**对所有帧进行掩码。去掩码算法（与 4 字节密钥 XOR）：

```cpp
for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] ^= masking_key[i % 4];
}
```

实现位置：`frame_parser.cc:97`

### 长度编码规则

- ≤ 125: 直接编码在第 2 个字节中
- 126-65535: 使用 2 字节扩展长度
- > 65535: 使用 8 字节扩展长度

## 命名规范

- **命名空间**: `darwincore::websocket::`, `darwincore::jsonrpc::`
- **类名**: 大驼峰 PascalCase (`JsonRpcServer`, `FrameParser`)
- **方法名**: 大驼峰 PascalCase (`StartServer`, `RegisterMethod`)
- **成员变量**: 小写+下划线+尾部下划线 (`connection_count_`, `recv_buffer_`)
- **常量**: `k` + 大驼峰 (`kMaxConnections`, `kText`)
- **局部变量**: 小写+下划线 (`connection_id`, `frame_data`)

## 测试

### 单元测试

位于 `test/` 目录，使用 Google Test 框架：

- `frame_parser_test.cc`: 测试各种操作码和长度的帧解析
- `frame_builder_test.cc`: 测试帧构建
- `handshake_handler_test.cc`: 测试 HTTP 升级握手
- `jsonrpc_test.cc`: 测试 JSON-RPC 请求路由和错误处理

### 手动测试

1. 启动示例服务器：
   ```bash
   cd build && ./examples/simple_server
   ```

2. 在浏览器中打开 `examples/client.html`
3. 点击 "Connect" 然后 "Call add(2, 3)" 测试

## 修改代码库指南

### 添加新的 JSON-RPC 方法

无需修改核心库，在应用代码中注册即可：

```cpp
server.RegisterMethod("myMethod", [](const nlohmann::json& params) {
    // 处理参数
    return nlohmann::json{{"result", value}};
});
```

### 添加新的 WebSocket 帧类型支持

例如添加分帧支持：

1. 在 `frame_parser.h` 添加 OpCode（如果需要）
2. 更新 `FrameParser::Parse()` 处理新帧类型
3. 更新 `jsonrpc_server.cc` 的 `HandleWebSocketFrame()` 处理它
4. 在 `frame_parser_test.cc` 添加测试

### 添加服务器回调

例如添加 `OnPingReceived` 回调：

1. 在 `JsonRpcServer` 类中添加 `std::function<>` 成员
2. 添加 `SetOn...()` 公共方法
3. 在适当位置调用回调
4. **重要**: 确保回调在锁外调用，防止死锁

## 常见问题

### 握手失败

检查：
- 请求中包含 `Sec-WebSocket-Key`
- `Sec-WebSocket-Version` 为 "13"
- handshake_handler.cc:28 的 GUID 魔术字符串正确

### 帧解析错误

常见原因：
- 客户端未掩码（违反 WebSocket 规范）
- 载荷长度与实际数据不匹配
- 操作码无效

在 `frame_parser.cc:38` 添加调试日志诊断。

### 死锁

如果遇到死锁，检查：
- 用户回调不在持有 `connections_mutex_` 时调用
- 锁作用域最小化（使用 `{ }` 创建作用域）
- 没有递归获取锁

参考上方的"线程安全关键点"。

## 代码规范

### 智能指针

- 使用 `std::unique_ptr` 管理独占所有权
- 使用 `std::shared_ptr` 当所有权共享时
- 绝不使用裸指针的 `new`/`delete`

### 错误处理

- 使用 `std::optional<T>` 表示可能失败但非异常的操作
- 对真正的异常情况抛出异常（网络失败、协议违规）
- 简单的成功/失败返回 bool

### 性能考虑

1. **缓冲区复用**: 接收缓冲区按需增长但从不收缩
2. **零拷贝**: 使用 `std::move` 传递大负载
3. **线程池**: DarwinCore 的 WorkerPool 处理并发请求
4. **锁竞争**: 最小化持有 `connections_mutex_` 的时间

## 参考资料

- RFC 6455: WebSocket Protocol https://tools.ietf.org/html/rfc6455
- JSON-RPC 2.0: https://www.jsonrpc.org/specification
- DarwinCore Network: 见 `../libs/darwincore/include/darwincore/network/`
