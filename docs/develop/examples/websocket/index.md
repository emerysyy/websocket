# WebSocket 示例

## 范围

纯 WebSocket 协议示例，展示底层 WebSocket 帧传输，不涉及 JSON-RPC。

## 文档

- [echo-server.md](./echo-server.md) - C++ 回显服务器
- [frame-test.md](./frame-test.md) - 浏览器帧测试工具

## 相关源码

```
examples/websocket/
├── echo_server.cc      # C++ 回显服务器
└── frame_test.html     # 浏览器帧测试工具
```

## 与 JSON-RPC 示例的区别

| 特性 | JSON-RPC 示例 | WebSocket 示例 |
|------|--------------|----------------|
| 协议 | JSON-RPC 2.0 | 原始 WebSocket 帧 |
| 数据格式 | JSON | 任意文本或二进制 |
| 业务逻辑 | RPC 方法调用 | 仅回显/转发 |
| 依赖 | `jsonrpc_server.h` | `frame_parser.h` + `frame_builder.h` |

## 协议栈对比

**JSON-RPC 示例**：
```
[应用层] JSON-RPC 2.0
    ↓
[传输层] WebSocket
```

**WebSocket 示例**：
```
[应用层] 业务逻辑（回显、转发等）
    ↓
[传输层] WebSocket 帧（FrameParser/FrameBuilder）
    ↓
[网络层] TCP/IP
```