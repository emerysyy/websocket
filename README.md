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

## 示例程序

### 目录结构

```
examples/
├── websocket/              # WebSocket 示例
│   ├── echo_server.cc      # WebSocket 回显服务器
│   └── frame_test.html     # 浏览器帧测试工具
├── jsonrpc/                # JSON-RPC 示例
│   ├── simple_server.cc    # JSON-RPC 服务器
│   ├── client.html         # 浏览器客户端
│   └── nodejs-client/      # Node.js 客户端
```

### 构建

```bash
# 启用 examples 构建
cmake .. -DCMAKE_BUILD_EXAMPLES=ON
make -j$(sysctl -n hw.ncpu)
```

### 运行

**WebSocket 回显服务器：**
```bash
./examples/websocket/echo_server
# 监听 ws://127.0.0.1:8080/echo
# 使用 frame_test.html 测试
```

**JSON-RPC 服务器：**
```bash
./examples/jsonrpc/simple_server
# 监听 ws://127.0.0.1:8080/jsonrpc
```

**浏览器客户端：**
- WebSocket: 直接打开 `examples/websocket/frame_test.html`
- JSON-RPC: 直接打开 `examples/jsonrpc/client.html`

**Node.js 客户端：**
```bash
cd examples/jsonrpc/nodejs-client
node client.js
```

## 参考顺序

1. 先读 `docs/refactor/`
2. 再按 `docs/develop/` 实现
3. 最后补 `test/`
