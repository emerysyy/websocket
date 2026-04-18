# WebSocket 开发索引

## 范围

WebSocket 层只负责协议和连接会话，不直接处理 JSON-RPC 业务。

## 核心组件

### WebSocketServer

`WebSocketServer` 是 WebSocket 层的统一入口，负责：
- 服务器生命周期管理（启动/停止）
- HTTP Upgrade 握手处理
- WebSocket 帧解析和路由
- 连接管理（使用 Connection 封装）

**头文件**：`include/darwincore/websocket/websocket_server.h`

**使用示例**：
```cpp
#include <darwincore/websocket/websocket_server.h>

using namespace darwincore::websocket;

WebSocketServer server;
server.SetOnConnected([](const ConnectionPtr& conn) {
  std::cout << "Client connected: " << conn->connection_id() << std::endl;
});

server.SetOnFrame([](const ConnectionPtr& conn, const Frame& frame) {
  if (frame.opcode == OpCode::kText) {
    std::string payload(frame.payload.begin(), frame.payload.end());
    std::cout << "Received: " << payload << std::endl;
    // 回复
    server.SendText(conn, "Echo: " + payload);
  }
});

server.SetOnDisconnected([](const ConnectionPtr& conn) {
  std::cout << "Client disconnected: " << conn->connection_id() << std::endl;
});

server.Start("0.0.0.0", 8080);
```

### Connection

`Connection` 是连接封装，继承 `WebSocketSession`：
- 一个连接对应一个 Connection 对象
- 会话状态挂在 Connection 自身
- 不维护全局连接表

**类型定义**：`using ConnectionPtr = std::shared_ptr<Connection>`

### WebSocketSession

`WebSocketSession` 是会话状态基类，提供：
- 阶段管理（kHandshake → kWebSocket → kClosing → kClosed）
- 帧解析器
- 关闭状态

## 文档列表

- [websocket-server.md](./websocket-server.md) - WebSocketServer 详细文档
- [handshake.md](./handshake.md) - 握手处理器
- [frame-parser.md](./frame-parser.md) - 帧解析器
- [frame-builder.md](./frame-builder.md) - 帧构建器
- [session.md](./session.md) - 会话状态
- [test-handshake.md](./test-handshake.md) - 握手测试
- [test-frame-parser.md](./test-frame-parser.md) - 帧解析测试
- [test-frame-builder.md](./test-frame-builder.md) - 帧构建测试
- [test-session.md](./test-session.md) - 会话测试
