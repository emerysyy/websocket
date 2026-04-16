# NotificationBuilder

## 目标

构建 JSON-RPC 通知消息。

## 公开接口

- `Create(const std::string&, const nlohmann::json&)`
- `Create(const std::string&)`

## 关键点

- 通知没有 `id`
- 输出应符合 JSON-RPC 2.0
- 只负责组包，不负责发送

## 相关实现

- `src/jsonrpc/notification_builder.cc`
- `include/darwincore/websocket/jsonrpc/notification_builder.h`

## 验收

- 带参数通知格式正确
- 无参数通知格式正确
- 不生成 `id`
