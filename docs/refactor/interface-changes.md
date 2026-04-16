# 业务接口调整

## 新接口（统一使用 ConnectionPtr）

重构后，所有对外接口统一使用 `ConnectionPtr`，不再提供 `uint64_t connection_id` 路径：

```cpp
// 发送通知到指定连接
bool SendNotification(const ConnectionPtr& conn, const std::string& method, const nlohmann::json& params);

// 广播到所有连接（由 Server::Broadcast 实现）
void BroadcastNotification(const std::string& method, const nlohmann::json& params);

// 关闭指定连接
bool CloseConnection(const ConnectionPtr& conn, uint16_t code = 1000, const std::string& reason = "");

// 连接事件回调
void SetOnClientConnected(std::function<void(const ConnectionPtr&)> cb);
void SetOnClientDisconnected(std::function<void(const ConnectionPtr&)> cb);
```

---

## 为什么不保留 id 兼容层

1. **所有权清晰**：`ConnectionPtr` 是 `shared_ptr`，生命周期由 DarwinCore 管理，业务层持有是安全的。
2. **避免双路径**：同时维护 `ConnectionPtr` 和 `id` 两套索引会增加复杂度，且容易出错。
3. **DarwinCore 设计**：新 API 本身就是以 `ConnectionPtr` 为核心，没有提供 id 查找接口。

如果业务层确实需要 id 索引（例如现有代码大量依赖 id），由业务层自行维护：

```cpp
// 业务层自行维护（不在框架层提供）
mutable std::mutex                          id_index_mutex_;
std::unordered_map<uint64_t, ConnectionPtr> id_index_;
```

---

## 接口对比

| 旧接口 | 新接口 |
|-------|-------|
| `SendNotification(uint64_t id, ...)` | `SendNotification(const ConnectionPtr&, ...)` |
| `SetOnClientConnected(std::function<void(uint64_t, ConnectionInformation)>)` | `SetOnClientConnected(std::function<void(const ConnectionPtr&)>)` |
| `SetOnClientDisconnected(std::function<void(uint64_t)>)` | `SetOnClientDisconnected(std::function<void(const ConnectionPtr&)>)` |
| `SetOnMessage(std::function<void(uint64_t, vector<uint8_t>)>)` | `SetMessageCallback(std::function<void(const ConnectionPtr&, Buffer&, Timestamp)>)` |
| `CloseConnection(uint64_t id)` | `CloseConnection(const ConnectionPtr&)` |

---

## 迁移建议

1. **优先修改内部实现**：先把 `JsonRpcServer` 内部改为 `ConnectionPtr` 路径。
2. **逐步替换业务代码**：业务层调用处从 `SendNotification(id, ...)` 改为 `SendNotification(conn, ...)`。
3. **临时兼容层（可选）**：如果业务层改动量大，可以在业务层封装一个 id → ConnectionPtr 的查找函数，但不建议长期保留。
