# frame_test

## 目标

浏览器 WebSocket 帧测试工具，用于手动测试 WebSocket 帧的收发。

## 相关文件

- `examples/websocket/frame_test.html`

## 运行方式

1. 启动 `echo_server`
2. 用浏览器打开 `frame_test.html`
3. 点击"连接"连接服务器

## 界面功能

### 连接控制

- **连接** - 连接到 `ws://127.0.0.1:9999`
- **断开** - 断开连接

### 发送帧

- **帧类型选择** - Text (0x1) 或 Binary (0x2)
- **载荷输入** - 输入要发送的内容
- **发送** - 发送自定义帧

### 控制帧

- **发送 Ping** - 发送 Ping 帧
- **发送 Close** - 发送 Close 帧并关闭

### 日志

显示接收到的帧：
- 发送的帧
- 接收的帧
- 控制消息（Ping/Pong/Close）
- 连接状态变化

## 与 browser-client（JSON-RPC）的区别

| 特性 | frame_test | browser-client |
|------|-----------|----------------|
| 功能 | 发送原始 WebSocket 帧 | 发送 JSON-RPC 请求 |
| 接收显示 | 显示原始帧内容 | 解析 JSON-RPC 响应/通知 |
| 用途 | 调试 WebSocket 协议 | 测试 JSON-RPC 功能 |
| 帧类型 | Text/Binary/Ping/Close | 仅 Text |

## 验收

- 能成功连接到 echo_server
- 能发送 Text 帧并收到回显
- 能发送 Binary 帧并收到回显
- 能发送和接收 Ping/Pong
- 能正确发送 Close 帧