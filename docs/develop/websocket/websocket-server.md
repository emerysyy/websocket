# WebSocketServer

## 概述

`WebSocketServer` 是 WebSocket 层的统一入口类，封装了 DarwinCore 网络层，提供完整的 WebSocket 服务器功能。

## 职责

- 服务器生命周期管理（启动/停止）
- HTTP Upgrade 握手处理
- WebSocket 帧解析和路由
- 连接管理（Connection 封装）

## 架构位置

```
┌─────────────────────────────────────────────────────────┐
│ 应用层                                                    │
│  用户代码直接使用 WebSocketServer                         │
├─────────────────────────────────────────────────────────┤
│ 协议层 (WebSocketServer)                                  │
│  WebSocketServer, HandshakeHandler, FrameParser          │
├─────────────────────────────────────────────────────────┤
│ 会话层                                                    │
│  WebSocketSession (Connection 继承)                      │
├─────────────────────────────────────────────────────────┤
│ 传输层                                                    │
│  DarwinCore EventLoopGroup + Server                      │
└─────────────────────────────────────────────────────────┘
```

## 头文件

```cpp
#include <darwincore/websocket/websocket_server.h>
```

## 核心类型

### ConnectionPtr

```cpp
using ConnectionPtr = std::shared_ptr<Connection>;
```

封装单个 WebSocket 连接，提供：
- `connection_id()` - 连接 ID
- `remote_address()` - 远程地址
- `IsConnected()` - 连接状态
- `phase()` - 当前阶段
- `recv_buffer()` - 接收缓冲区

### FrameCallback

```cpp
using FrameCallback = std::function<void(const ConnectionPtr& conn, const Frame& frame)>;
```

帧回调类型，用于处理接收到的 WebSocket 帧。

## 公共接口

### 生命周期

```cpp
// 启动服务器
bool Start(const std::string& host, uint16_t port);

// 停止服务器
void Stop();

// 检查是否运行中
bool IsRunning() const;

// 获取当前连接数
size_t GetConnectionCount() const;
```

### 发送数据

```cpp
// 发送 WebSocket 帧
bool SendFrame(const ConnectionPtr& conn,
               const std::vector<uint8_t>& payload,
               OpCode opcode = OpCode::kText);

// 发送文本帧
bool SendText(const ConnectionPtr& conn, const std::string& text);

// 发送二进制帧
bool SendBinary(const ConnectionPtr& conn, const std::vector<uint8_t>& data);

// 发送 Ping 帧
bool SendPing(const ConnectionPtr& conn,
              const std::vector<uint8_t>& payload = {});

// 发送 Pong 帧
bool SendPong(const ConnectionPtr& conn,
              const std::vector<uint8_t>& payload = {});
```

### 关闭连接

```cpp
// 发送 Close 帧并进入关闭阶段（异步关闭）
bool Close(const ConnectionPtr& conn, uint16_t code = 1000,
           const std::string& reason = "");

// 强制立即关闭连接（同步关闭）
void ForceClose(const ConnectionPtr& conn, uint16_t code = 1000,
                const std::string& reason = "");
```

**Close() vs ForceClose()**：
- `Close()` - 发送 Close 帧后进入关闭阶段，等待对方响应。连接仍计入 GetConnectionCount()，直到收到对方 Close 或超时。
- `ForceClose()` - 先发送 Close 帧通知对端，然后立即清理本地状态并触发 `on_disconnected_`。适用于服务器主动终止连接的场景。
- **幂等性**：两者都是幂等的，重复调用不会重复发送 Close 帧或触发回调。

**发送前检查**：SendFrame、SendText、SendBinary、SendPing、SendPong 等发送方法在发送前会检查 `is_closing()` 状态，正在关闭的连接返回 false。

### 广播

```cpp
// 广播到所有已升级连接
size_t Broadcast(const std::vector<uint8_t>& payload,
                 OpCode opcode = OpCode::kText);
```

**语义**：只广播已升级到 WebSocket 阶段的连接（`phase() == kWebSocket`），不包括握手阶段的连接。

### 回调设置

```cpp
// 设置帧处理回调
void SetOnFrame(FrameCallback callback);

// 设置连接建立回调
void SetOnConnected(std::function<void(const ConnectionPtr&)> callback);

// 设置连接断开回调
void SetOnDisconnected(std::function<void(const ConnectionPtr&)> callback);

// 设置错误回调
void SetOnError(std::function<void(const ConnectionPtr&, const std::string&)> callback);
```

## 使用示例

### 基本回显服务器

```cpp
#include <iostream>
#include <csignal>
#include <darwincore/websocket/websocket_server.h>

using namespace darwincore::websocket;

volatile sig_atomic_t g_running = true;
void SignalHandler(int) { g_running = 0; }

int main() {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    WebSocketServer server;

    server.SetOnConnected([](const ConnectionPtr& conn) {
        std::cout << "[+] Connected: " << conn->connection_id()
                  << " from " << conn->remote_address() << std::endl;
    });

    server.SetOnFrame([&server](const ConnectionPtr& conn, const Frame& frame) {
        if (frame.opcode == OpCode::kText) {
            std::string payload(frame.payload.begin(), frame.payload.end());
            std::cout << "[>] " << payload << std::endl;

            // 回显
            server.SendText(conn, payload);
        } else if (frame.opcode == OpCode::kPing) {
            server.SendPong(conn, frame.payload);
        }
    });

    server.SetOnDisconnected([](const ConnectionPtr& conn) {
        std::cout << "[-] Disconnected: " << conn->connection_id() << std::endl;
    });

    if (!server.Start("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.Stop();
    return 0;
}
```

### 聊天室服务器

```cpp
WebSocketServer server;

// 连接时加入房间
server.SetOnConnected([](const ConnectionPtr& conn) {
    std::cout << "User joined: " << conn->connection_id() << std::endl;
});

// 广播消息给所有用户
server.SetOnFrame([&server](const ConnectionPtr& conn, const Frame& frame) {
    if (frame.opcode == OpCode::kText) {
        std::string msg = "User " + std::to_string(conn->connection_id()) +
                          ": " + std::string(frame.payload.begin(),
                                            frame.payload.end());
        server.Broadcast(std::vector<uint8_t>(msg.begin(), msg.end()),
                         OpCode::kText);
    }
});

// 移除用户
server.SetOnDisconnected([](const ConnectionPtr& conn) {
    std::string msg = "User left: " + std::to_string(conn->connection_id());
    // 广播离开消息
});

server.Start("0.0.0.0", 8080);
```

## 连接生命周期

```
[新连接到达]
      ↓
[Server 创建 Connection，绑定到 connections_]
      ↓
[触发 SetOnConnected 回调]
      ↓
[开始接收数据...]
      ↓
[收到完整 HTTP 请求]
      ↓
[HandshakeHandler 处理握手]
      ↓
[切换到 kWebSocket 阶段]
      ↓
[收到 WebSocket 帧]
      ↓
[触发 SetOnFrame 回调]
      ↓
[... 正常通信 ...]
      ↓
[收到 Close 帧或连接断开]
      ↓
[触发 SetOnDisconnected 回调]
      ↓
[Connection 从 connections_ 移除]
      ↓
[Connection 析构]
```

## OpCode 参考

| OpCode | 值 | 说明 |
|--------|-----|------|
| kContinuation | 0x0 | 延续帧 |
| kText | 0x1 | 文本帧 |
| kBinary | 0x2 | 二进制帧 |
| kClose | 0x8 | 关闭帧 |
| kPing | 0x9 | Ping 帧 |
| kPong | 0xA | Pong 帧 |

## 线程安全

- `Start()` / `Stop()` 可从主线程调用
- 回调在网络 I/O 线程执行
- 发送方法（`SendText`, `SendFrame` 等）是线程安全的
- `ConnectionPtr` 可安全地在回调间传递

## 相关文件

- `include/darwincore/websocket/websocket_server.h`
- `src/websocket/websocket_server.cc`
