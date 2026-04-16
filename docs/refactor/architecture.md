# 目标架构

## 分层

```
┌─────────────────────────────────────────────────────────┐
│ 应用层                                                    │
│  JsonRpcServer, RequestHandler, NotificationBuilder      │
├─────────────────────────────────────────────────────────┤
│ 协议层                                                    │
│  HandshakeHandler, FrameParser, FrameBuilder             │
├─────────────────────────────────────────────────────────┤
│ 会话层                                                    │
│  WebSocketSession (挂载在 Connection::context_)          │
├─────────────────────────────────────────────────────────┤
│ 传输层                                                    │
│  DarwinCore EventLoopGroup + Server                      │
└─────────────────────────────────────────────────────────┘
```

## 核心原则

- **一个连接对应一个 session**：每个 `Connection` 绑定一个 `WebSocketSession`（通过 `Connection::SetContext()`）
- **session 状态挂在 Connection 上**：不再维护全局连接表
- **WebSocket 解析在 I/O 线程内完成**：不加额外锁
- **发送使用 `conn->Send()`**：广播使用 `server_->Broadcast()` 或遍历连接集合

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
├── jsonrpc_server.h
├── frame_parser.h
└── session.h                # 新增

src/
├── jsonrpc_server.cc
└── frame_parser.cc

CMakeLists.txt
test/
├── test_jsonrpc_server_lifecycle.cc
├── test_websocket_session.cc
├── test_frame_parser_buffer.cc
├── test_broadcast.cc
└── test_graceful_shutdown.cc
```
