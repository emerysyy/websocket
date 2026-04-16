# websocket

基于 DarwinCore Network 的 WebSocket + JSON-RPC 项目。

## 当前文档结构

- `docs/refactor/`：重构规格、迁移约束、验收标准
- `docs/develop/`：按 `websocket`、`jsonrpc`、`build` 拆分的开发和测试文档

## 目标分层

- `include/darwincore/websocket/`：WebSocket 公开 API
- `include/darwincore/jsonrpc/`：JSON-RPC 公开 API
- `src/`：实现
- `test/`：单元测试
- `examples/`：示例程序

## 构建

```bash
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## 测试

```bash
cd build
./test/run_tests
```

单个测试：

```bash
cd build
./test/run_tests --gtest_filter=FrameParserTest.ParseSimpleTextFrame
```

## 运行示例

```bash
cd build
./examples/simple_server
```

服务器默认监听 `ws://127.0.0.1:9998`。

## 参考顺序

1. 先读 `docs/refactor/`
2. 再按 `docs/develop/` 实现
3. 最后补 `test/`
