# echo_server

## 目标

纯 WebSocket 回显服务器，只处理 WebSocket 帧，不涉及 JSON-RPC。

## 相关文件

- `examples/websocket/echo_server.cc`

## 运行方式

```bash
cd build
make echo_server  # 如果有 CMake 配置
./bin/echo_server
```

服务器监听 `ws://127.0.0.1:9999`

## 功能

| 功能 | 说明 |
|------|------|
| 文本帧回显 | 收到文本帧后原样返回 |
| 二进制帧回显 | 收到二进制帧后原样返回 |
| Ping 处理 | 收到 Ping 自动回复 Pong |
| Close 处理 | 收到 Close 帧后断开连接 |

## 连接上下文

每个连接维护一个 `ConnectionContext`：

```cpp
struct ConnectionContext {
    std::unique_ptr<FrameParser> parser;  // 帧解析器
    std::vector<uint8_t> recv_buffer;    // 接收缓冲区
    bool connected = false;
};
```

## 帧处理流程

```
[接收数据] → [追加到缓冲区] → [循环解析帧] → [处理帧] → [回显/响应]
                                         ↓
                                   [数据不完整] → 等待更多数据
```

## 使用的组件

- `DarwinCore::Server` - 网络服务器
- `FrameParser` - 帧解析
- `FrameBuilder` - 帧构建（Ping/Pong/Close）

## 验收

- 能正常启动并监听 9999 端口
- 文本帧能正确回显
- 二进制帧能正确回显
- Ping 帧能正确响应 Pong
- Close 帧能正确关闭连接