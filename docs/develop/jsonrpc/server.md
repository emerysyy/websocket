# JsonRpcServer

## 目标

把 WebSocket 和 JSON-RPC 组合成可用服务器。

## 公开接口方向

- `Start(...)`
- `Stop()`
- `GetConnectionCount()`
- `RegisterMethod(...)`
- `SendNotification(...)`
- `BroadcastNotification(...)`
- `CloseConnection(...)`
- `SetOnClientConnected(...)`
- `SetOnClientDisconnected(...)`

## 关键点

- 公开 API 使用 `ConnectionPtr`
- 会话状态挂在 `Connection::context_`
- 不维护全局 `connections_mutex_`
- 收到 WebSocket 帧后再路由到 JSON-RPC

## 相关实现

- `src/jsonrpc_server.cc`
- `include/darwincore/websocket/jsonrpc_server.h`

## 验收

- 能启动和停止
- 能收发 JSON-RPC 消息
- 能广播
- 能安全关闭连接
