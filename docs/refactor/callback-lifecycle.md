# 回调生命周期

## 连接回调（`ConnectionCallback`）

### 连接建立（`conn->IsConnected() == true`）

1. 创建 `WebSocketSession`（`shared_ptr`），通过 `conn->SetContext(session)` 绑定。
2. 初始化协议状态（phase = kHandshake）。
3. 将 `ConnectionPtr` 加入应用层连接索引（如需要 id 查找）。
4. 触发用户回调 `on_connected_`。

### 连接断开（`conn->IsConnected() == false`）

1. 从应用层连接索引中移除该连接。
2. 清理 session（`shared_ptr` 引用计数归零时自动释放）。
3. 触发用户回调 `on_disconnected_`。

---

## 消息回调（`MessageCallback`）

消息回调必须满足：

- 在连接所属 I/O 线程中执行（DarwinCore 保证）。
- 允许直接访问 session（无锁，因为同一连接的消息串行处理）。
- 不依赖全局锁保护协议状态。
- 不能在回调内部阻塞等待其他连接状态。

---

## 用户回调

用户回调只能在内部状态更新完成后触发。

触发后允许用户立即调用：

- `GetConnectionCount()`
- `SendNotification(conn, ...)`
- `BroadcastNotification()`
- `CloseConnection()`

因此内部实现不能在调用用户回调时持有会被这些接口再次获取的锁。
