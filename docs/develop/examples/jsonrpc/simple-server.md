# simple_server

## 目标

C++ 实现的 WebSocket + JSON-RPC 服务器示例。

## 相关文件

- `examples/simple_server.cc`

## 运行方式

```bash
cd build
./bin/simple_server
```

服务器监听 `ws://127.0.0.1:9998`

## 注册的 RPC 方法

| 方法 | 参数 | 说明 |
|------|------|------|
| `echo` | 任意 JSON | 返回接收到的参数 |
| `add` | `{"a": num, "b": num}` | 加法 |
| `multiply` | `{"a": num, "b": num}` | 乘法 |
| `getTime` | 无 | 返回服务器时间戳 |
| `getConnectionCount` | 无 | 返回当前连接数 |

## 事件回调

- 连接时打印：`[+] Client connected: <id> from <address>:<port>`
- 断开时打印：`[-] Client disconnected: <id>`
- 错误时打印：`[!] Connection error <id>: <message>`

## 心跳广播

每 30 秒向所有客户端广播 `heartbeat` 通知：

```json
{
    "jsonrpc": "2.0",
    "method": "heartbeat",
    "params": {
        "count": <序号>,
        "connections": <连接数>
    }
}
```

## 退出

按 `Ctrl+C`，服务器会优雅关闭并发送关闭帧。

## 验收

- 能正常启动并监听 9998 端口
- RPC 方法调用正常返回
- 心跳广播正常
- 能处理 Ctrl+C 退出