# 迁移步骤

## 1. 引入 session 模型

新增 `WebSocketSession`，作为连接上下文对象，挂载到 `Connection::context_`。

```cpp
// include/darwincore/websocket/session.h
struct WebSocketSession {
    ConnectionPhase phase   = ConnectionPhase::kHandshake;
    FrameParser     parser;
    bool            closing = false;
    // recv_buffer 不再需要：直接读 Connection::InputBuffer()（即 MessageCallback 的 Buffer&）
};
```

要求：

- `WebSocketSession` 只保存该连接的协议状态，不持有 `recv_buffer`（由 DarwinCore `Buffer` 管理）。
- 任何连接专属状态都放入 session。
- 不再使用 `connections_mutex_` 和 `connections_` map。

---

## 2. 改造 `JsonRpcServer`

`JsonRpcServer` 直接持有：

```cpp
EventLoopGroup              loop_group_;   // 先于 server_ 构造
std::unique_ptr<network::Server> server_;
```

`Start()` 的执行顺序必须固定为：

1. 构造 `EventLoopGroup(thread_count)` 并调用 `loop_group_.Start()`。
2. 构造 `network::Server(loop_group_)`。
3. 注册 `SetConnectionCallback`。
4. 注册 `SetMessageCallback`。
5. 调用 `server_->StartIPv4(host, port)` 开始监听。

`Stop()` 的执行顺序必须固定为：

1. 标记 `is_running_ = false`（防止新请求被处理）。
2. 调用 `server_->Stop()`（停止接受新连接）。
3. 调用 `loop_group_.Stop()`（等待所有 I/O 线程退出，此后无回调再触发）。
4. `server_.reset()` 释放服务器对象，确保其析构发生在所有回调停止之后。

> **注意**：旧代码在 `Stop()` 之后还尝试发送 Close 帧，这是错误的——`Server::Stop()` 之后连接已失效。正确做法是在收到 WebSocket Close 帧时，由 `OnMessage` 回调内主动调用 `conn->ShutdownWrite()` 或 `conn->ForceClose()`。

---

## 3. 改造消息入口

`MessageCallback` 签名：`void(const ConnectionPtr&, Buffer&, Timestamp)`

进入后，只做两件事：

1. 从 `Connection::GetContext()` 读取 `WebSocketSession`：
   ```cpp
   auto session = std::any_cast<std::shared_ptr<WebSocketSession>>(conn->GetContext());
   ```
2. 将 `Buffer` 里的数据交给握手或帧解析流程。

处理规则：

- **握手阶段**：在 `Buffer::Peek()` 所指数据中查找 `\r\n\r\n`；找到后提取 HTTP 请求字符串，调用 `HandshakeHandler`，握手成功后 `Retrieve()` 消费握手数据，切换 session 到 `kWebSocket` 阶段。
- **WebSocket 阶段**：循环调用 `FrameParser::Parse(buf.Peek(), buf.ReadableBytes(), consumed)`；每次解析成功后 `buf.Retrieve(consumed)`；数据不足则退出循环，等待下次回调。
- **大小保护**：握手阶段检查 `buf.ReadableBytes() > kMaxHandshakeSize`，超出则 `conn->ForceClose()`；WebSocket 阶段检查 `payload_length > kMaxWebSocketFrameSize`，超出则 `conn->ForceClose()`。

---

## 4. 改造 `FrameParser`

`FrameParser` 保留现有帧语义，但增加零拷贝输入接口：

```cpp
// 新增：直接接受原始指针，避免构造 vector<uint8_t>
std::optional<Frame> Parse(const uint8_t* data, size_t len, size_t& consumed);
```

原有 `Parse(const std::vector<uint8_t>&, size_t&)` 保留为兼容入口，内部直接转调新接口。
两者关系是：

- 外部新路径优先使用 `Parse(const uint8_t*, size_t, size_t&)`
- 旧测试和旧调用可继续走 `std::vector<uint8_t>` 版本
- 两个接口必须共享同一套解析逻辑，不能出现双实现

要求：

- 解析函数必须能够处理分片到达的数据。
- 解析函数必须返回本次消耗字节数。
- 掩码解码仍在解析阶段完成。
- 解析失败时不得破坏后续可恢复数据。

---

## 5. 改造发送路径

所有发送都走 `ConnectionPtr`：

```cpp
conn->Send(frame.data(), frame.size());
```

广播有两种方式：

- **直接广播**（无差别）：`server_->Broadcast(data, size)`，由 Server 内部遍历连接表。
- **有条件广播**（仅 WebSocket 阶段连接）：暂不能使用 `Broadcast`，需要在应用层维护一个 `std::set<ConnectionPtr>` 或类似结构，遍历发送。建议在 `ConnectionCallback` 建立/断开时维护此集合，用 `loop_group_` 的某个 I/O loop 线程专门操作（避免加锁），或者用 `std::mutex` 保护。

要求：

- 不再通过 connection_id 反向调用网络层发送接口。
- **移除** `SendNotification(uint64_t id, ...)` 接口，统一使用 `ConnectionPtr` 主路径。
- 如果业务层确实需要 id 查找，由业务层自行维护 `id → ConnectionPtr` 映射（加锁保护），不在框架层提供。

---

## 6. 改造关闭流程

关闭连接时必须区分：

- **优雅关闭**（正常 WebSocket Close 握手）：
  1. 发送 WebSocket Close 帧：`conn->Send(close_frame)`。
  2. 调用 `conn->ShutdownWrite()`，等待对端关闭。
  3. 对端关闭后 DarwinCore 会自动触发 `ConnectionCallback`（`IsConnected() == false`）。

- **异常关闭**（协议错误、超大消息、握手失败）：
  1. 直接调用 `conn->ForceClose()`，DarwinCore 触发 `HandleClose`，随后触发 `ConnectionCallback`。

关闭流程必须避免在回调内重复关闭同一连接：
- 在 `WebSocketSession::closing` 置 `true` 后，后续消息直接忽略。
- `ConnectionCallback`（断开侧）清理 session 时检查 `closing` 状态。
