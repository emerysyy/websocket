# JSON-RPC 示例

## 范围

JSON-RPC 示例展示基于 WebSocket 传输的 JSON-RPC 2.0 协议功能。

## 文档

- [simple-server.md](./simple-server.md) - C++ 服务端
- [browser-client.md](./browser-client.md) - 浏览器客户端
- [nodejs-client.md](./nodejs-client.md) - Node.js 客户端

## 相关源码

```
examples/
├── simple_server.cc        # C++ 服务端
├── client.html              # 浏览器客户端
└── nodejs-client/
    ├── client.js            # Node.js 客户端
    └── package.json
```

## 协议栈

```
[应用层] JSON-RPC 2.0
    ↓
[传输层] WebSocket
    ↓
[网络层] TCP/IP
```