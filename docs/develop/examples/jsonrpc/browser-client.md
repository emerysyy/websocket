# client.html

## 目标

浏览器 WebSocket 客户端，用于手动测试 JSON-RPC 调用。

## 相关文件

- `examples/client.html`

## 运行方式

1. 启动 `simple_server`
2. 用浏览器打开 `client.html`（或通过 HTTP 服务器访问）
3. 点击 `Connect` 连接

## 界面功能

### 连接控制

- **Connect** - 连接到 `ws://127.0.0.1:9998`
- **Disconnect** - 断开连接

### RPC 调用

预设测试按钮：
- **Echo** - 发送随机数据并接收回显
- **Add (2 + 3)** - 固定参数加法
- **Multiply (4 × 5)** - 固定参数乘法
- **Get Time** - 获取服务器时间
- **Custom Add** - 使用下方输入框的自定义参数

### 日志区域

显示发送/接收的消息和通知：
- 📤 SEND - 发送的请求
- 📥 RECV - 收到的响应
- 🔔 NOTIFY - 服务端通知
- ❌ ERROR - 错误信息

## 发送的请求格式

```json
{
    "jsonrpc": "2.0",
    "id": <递增序号>,
    "method": "<方法名>",
    "params": <参数>
}
```

## 接收的消息类型

**RPC 响应**：
```json
{"jsonrpc": "2.0", "result": <结果>, "id": <id>}
```

**错误响应**：
```json
{"jsonrpc": "2.0", "error": {"code": <code>, "message": <msg>}, "id": <id>}
```

**服务端通知**：
```json
{"jsonrpc": "2.0", "method": "<方法>", "params": <参数>}
```

## 验收

- 能成功连接到服务器
- 预设按钮发送的 RPC 调用正常
- 自定义参数输入正常
- 心跳通知正常显示
- 响应和错误正确解析和显示