# HandshakeHandler

## 目标

实现 HTTP Upgrade 到 WebSocket 的握手流程。

## 公开接口

- `ParseRequest(const std::string&)`
- `GenerateResponse()`
- `GenerateErrorResponse(HandshakeError)`
- `IsValid()`
- `GetLastError()`
- `WebSocketKey()`
- `RequestUri()`
- `SetSupportedProtocols(...)`
- `NegotiatedProtocol()`

## 关键点

- 校验 `GET`、`HTTP/1.1`、`Upgrade`、`Connection`、`Sec-WebSocket-Version`、`Sec-WebSocket-Key`
- 计算 `Sec-WebSocket-Accept`
- 支持子协议协商
- 请求过大或格式错误时返回错误响应

## 相关实现

- `src/handshake_handler.cc`
- `include/darwincore/websocket/handshake_handler.h`

## 验收

- 合法握手返回 101
- 非法握手返回 400
- 子协议协商结果正确
