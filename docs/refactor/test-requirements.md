# 测试要求

必须补齐以下测试：

---

## 1. 生命周期测试

**文件**：`test_jsonrpc_server_lifecycle.cc`

测试内容：
- `Start()` 成功启动
- `Stop()` 正常停止
- 重复启动/停止（幂等性）
- 未 Start 就 Stop（安全处理）

---

## 2. Session 测试

**文件**：`test_websocket_session.cc`

测试内容：
- `Connection` 上下文挂载和读取
- Session 状态切换（kHandshake → kWebSocket）
- Session 在连接断开时正确清理

---

## 3. 解析测试

**文件**：`test_frame_parser_buffer.cc`

测试内容：
- `FrameParser` 处理原始指针输入
- 分段到达的数据（分片帧）
- 零拷贝路径正确性

---

## 4. 广播测试

**文件**：`test_broadcast.cc`

测试内容：
- 多连接并发发送
- 仅对 kWebSocket 阶段连接广播
- 广播时连接断开的安全处理

---

## 5. 关闭测试

**文件**：`test_graceful_shutdown.cc`

测试内容：
- 正常关闭（Close 帧 + ShutdownWrite）
- 异常关闭（ForceClose）
- 关闭时不重复操作同一连接
- 幂等性保护（closing 标志）
