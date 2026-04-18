# 目标架构

## 分层

```
┌─────────────────────────────────────────────────────────┐
│ 应用层                                                    │
│  WebSocketServer (用户入口), JsonRpcServer               │
├─────────────────────────────────────────────────────────┤
│ 协议层                                                    │
│  WebSocketServer, HandshakeHandler, FrameParser          │
│  FrameBuilder, JsonRpcServer                             │
├─────────────────────────────────────────────────────────┤
│ 会话层                                                    │
│  Connection (继承 WebSocketSession)                      │
├─────────────────────────────────────────────────────────┤
│ 传输层                                                    │
│  DarwinCore EventLoopGroup + Server                      │
└─────────────────────────────────────────────────────────┘
```

## 核心组件

### WebSocketServer

WebSocket 层的统一入口类，负责：
- 服务器生命周期管理
- HTTP Upgrade 握手处理
- WebSocket 帧解析和路由
- 连接管理

**用户直接使用 WebSocketServer**，不需要直接操作底层网络组件。

### Connection

封装单个 WebSocket 连接，继承 `WebSocketSession`：
- `connection_id()` - 连接 ID
- `remote_address()` - 远程地址
- `phase()` - 当前阶段
- `parser()` - 帧解析器

## 核心原则

- **WebSocketServer 是用户入口**：通过 WebSocketServer 管理所有连接
- **一个连接对应一个 Connection**：每个 `Connection` 继承 `WebSocketSession`
- **session 状态挂在 Connection 上**：不再维护全局连接表
- **WebSocket 解析在 I/O 线程内完成**：不加额外锁
- **发送使用 `server->SendFrame()`**：广播使用 `server->Broadcast()`

## 连接生命周期

```
[新连接到达]
      ↓
[Server 创建 Connection]
      ↓
[触发 ConnectionCallback - 建立]
      ↓
[创建 WebSocketSession，绑定到 Connection::context_]
      ↓
[触发用户 on_connected_ 回调]
      ↓
[开始接收数据...]
      ↓
[触发 MessageCallback - 数据到达]
      ↓
[根据 phase 分发到 Handshake 或 FrameParser]
      ↓
[... 正常通信 ...]
      ↓
[收到 Close 帧或连接断开]
      ↓
[触发 ConnectionCallback - 断开]
      ↓
[清理 session，触发用户 on_disconnected_ 回调]
      ↓
[Connection 析构，session 自动释放]
```

## 需要修改的文件

```
include/darwincore/websocket/
├── websocket_server.h       # WebSocket 统一入口 [已完成]
├── handshake_handler.h
├── frame_parser.h
├── frame_builder.h
└── session.h

src/websocket/
├── websocket_server.cc      # WebSocket 统一入口 [已完成]
├── handshake_handler.cc
├── frame_parser.cc
├── frame_builder.cc
└── session.cc

include/darwincore/jsonrpc/
├── jsonrpc_server.h         # 建立在 WebSocketServer 之上
└── ...

test/websocket/
├── test_websocket_server.cc  # 待新增
├── test_handshake.cc
├── test_frame_parser.cc
└── test_frame_builder.cc
```
