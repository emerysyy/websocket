# AGENTS.md

This file provides guidance to Codex when working in this repository.

## 项目定位

这是一个基于 DarwinCore Network 的 WebSocket + JSON-RPC 项目。当前权威文档分为两层：

- `docs/refactor/`：重构规格、迁移约束、验收标准
- `docs/develop/`：按 `websocket`、`jsonrpc`、`build` 拆分的开发和测试文档

## 代码边界

- `include/darwincore/websocket/` 只放 WebSocket 公开 API
- `include/darwincore/jsonrpc/` 只放 JSON-RPC 公开 API
- `src/` 只放实现
- `test/` 只放测试

## 工作原则

- 先对齐 `docs/refactor/`，再按 `docs/develop/` 落地。
- 不要把公开接口继续放在 `src/`。
- 不要重新引入旧的全局连接表、`connections_mutex_`、`recv_buffer_` 方案。
- WebSocket 层不直接耦合 JSON-RPC 业务，JSON-RPC 层建立在 WebSocket 层之上。

## 线程安全

- DarwinCore 回调在工作线程中执行。
- 用户回调必须在锁外调用。
- 会话状态应挂在 `Connection::context_`，而不是集中在服务器对象里。

## 构建与测试

```bash
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
./test/run_tests
```

单测优先顺序：

1. WebSocket 协议相关测试
2. JSON-RPC 相关测试
3. 构建和集成测试

## 文档优先级

1. `docs/refactor/`
2. `docs/develop/`
3. 根目录 `README.md`
