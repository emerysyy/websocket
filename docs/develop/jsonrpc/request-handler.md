# RequestHandler

## 目标

处理 JSON-RPC 2.0 请求、通知和批量请求。

## 公开接口

- `RegisterMethod(...)`
- `HandleRequest(...)`
- `HandleBatch(...)`
- `RegisteredMethods()`

## 关键点

- 校验 `jsonrpc == "2.0"`
- 支持单请求和批量请求
- 通知不返回响应
- 方法不存在、参数无效、解析失败要返回标准错误码
- **错误响应中的 id 字段**：只有当请求包含有效 id（非 null）时才包含在响应中。id 为 null 或不存在时不包含 id 字段，符合 JSON-RPC 2.0 规范

## 相关实现

- `src/jsonrpc/request_handler.cc`
- `include/darwincore/websocket/jsonrpc/request_handler.h`

## 验收

- 方法注册正常
- 单请求响应正常
- 批量请求响应正常
- 错误码符合 JSON-RPC 2.0
