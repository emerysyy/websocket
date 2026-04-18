# websocket

基于 DarwinCore Network 的 WebSocket + JSON-RPC 项目。

## 核心架构

```
┌─────────────────────────────────────────────────────────┐
│ 应用层                                                    │
│  WebSocketServer (用户入口)                              │
├─────────────────────────────────────────────────────────┤
│ 协议层                                                    │
│  WebSocketServer, HandshakeHandler, FrameParser          │
├─────────────────────────────────────────────────────────┤
│ 会话层                                                    │
│  Connection (继承 WebSocketSession)                     │
├─────────────────────────────────────────────────────────┤
│ 传输层                                                    │
│  DarwinCore EventLoopGroup + Server                     │
└─────────────────────────────────────────────────────────┘
```

## 核心组件

| 组件 | 说明 |
|------|------|
| **WebSocketServer** | WebSocket 统一入口，管理服务器生命周期、握手、帧路由 |
| Connection | 连接封装，继承 WebSocketSession |
| WebSocketSession | 会话状态基类 |
| HandshakeHandler | HTTP Upgrade 握手处理 |
| FrameParser | WebSocket 帧解析 |
| FrameBuilder | WebSocket 帧构建 |

## 文档结构

- `docs/refactor/`：重构规格、迁移约束、验收标准
- `docs/develop/websocket/`：WebSocket 开发文档
- `docs/develop/jsonrpc/`：JSON-RPC 开发文档

## 头文件

```cpp
#include <darwincore/websocket/websocket_server.h>
```

## 快速开始

```cpp
#include <darwincore/websocket/websocket_server.h>

using namespace darwincore::websocket;

WebSocketServer server;

server.SetOnFrame([&server](const ConnectionPtr& conn, const Frame& frame) {
    if (frame.opcode == OpCode::kText) {
        std::string payload(frame.payload.begin(), frame.payload.end());
        std::cout << "Received: " << payload << std::endl;
        server.SendText(conn, "Echo: " + payload);
    }
});

server.Start("0.0.0.0", 8080);
```

## 构建

```bash
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## 测试

```bash
cd build
./test/run_tests
```

单个测试：

```bash
cd build
./test/run_tests --gtest_filter=FrameParserTest.ParseSimpleTextFrame
```

## 示例程序

### 目录结构

```
examples/
├── websocket/              # WebSocket 示例
│   ├── echo_server.cc      # WebSocket 回显服务器
│   └── frame_test.html     # 浏览器帧测试工具
├── jsonrpc/                # JSON-RPC 示例
│   ├── simple_server.cc    # JSON-RPC 服务器
│   ├── client.html         # 浏览器客户端
│   └── nodejs-client/      # Node.js 客户端
```

### 构建

```bash
# 启用 examples 构建
cmake .. -DCMAKE_BUILD_EXAMPLES=ON
make -j$(sysctl -n hw.ncpu)
```

### 运行

**WebSocket 回显服务器：**
```bash
./examples/websocket/echo_server
# 监听 ws://127.0.0.1:8080/echo
# 使用 frame_test.html 测试
```

**JSON-RPC 服务器：**
```bash
./examples/jsonrpc/simple_server
# 监听 ws://127.0.0.1:8080/jsonrpc
```

**浏览器客户端：**
- WebSocket: 直接打开 `examples/websocket/frame_test.html`
- JSON-RPC: 直接打开 `examples/jsonrpc/client.html`

**Node.js 客户端：**
```bash
cd examples/jsonrpc/nodejs-client
node client.js
```

## 参考顺序

1. 先读 `docs/refactor/`
2. 再按 `docs/develop/` 实现
3. 最后补 `test/`
