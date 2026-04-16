# Handshake 测试

## 目标

验证握手处理的协议正确性。

## 覆盖点

- 有效握手
- 缺少 `Upgrade`
- 缺少 `Connection`
- 缺少 `Sec-WebSocket-Key`
- `Sec-WebSocket-Version` 错误
- 错误响应格式

## 相关测试

- `test/handshake_handler_test.cc`
