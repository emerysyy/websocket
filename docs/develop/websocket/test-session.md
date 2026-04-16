# WebSocketSession 测试

## 目标

验证会话状态挂载与清理。

## 覆盖点

- `Connection::SetContext()` / `GetContext()`
- 阶段切换
- 关闭标志
- 断开清理

## 相关测试

- 新增 `test/websocket_session_test.cc`
