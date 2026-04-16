# WebSocketSession

## 目标

把单连接的协议状态挂到 `Connection::context_` 上。

## 需要保存的状态

- 当前阶段
- 帧解析器
- 关闭状态

## 设计原则

- 一个连接一个 session
- 不维护全局连接表
- 不保存接收缓冲区
- 连接断开时自动释放

## 相关实现

- `include/darwincore/websocket/session.h`
- 目前以头文件状态容器为主，后续若需要行为再新增 `src/websocket/session.cc`

## 验收

- 能挂载到 `Connection`
- 能在连接内读取和更新
- 断开后正确清理
