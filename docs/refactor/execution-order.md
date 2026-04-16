# 执行顺序

## 9 步执行计划

### 第 1 步：修正 CMakeLists.txt
- 更新 DarwinCore 路径为 `3rd/`
- 确认能编译链接
- **产出**：可编译通过的 CMake 配置

### 第 2 步：新增 `session.h`
- 落地 `WebSocketSession` 定义
- **产出**：`include/darwincore/websocket/session.h`

### 第 3 步：改造 `JsonRpcServer`
- 引入 `EventLoopGroup` 成员
- 替换 `Start()` / `Stop()` 实现
- 移除旧回调注册，改用 `SetConnectionCallback` / `SetMessageCallback`
- **产出**：适配新 API 的 `JsonRpcServer`

### 第 4 步：改造消息入口
- `OnNetworkMessage` → `MessageCallback`
- 消除 `recv_buffer`
- 改用 `Buffer::Peek()` + `Retrieve()`
- **产出**：零拷贝消息处理

### 第 5 步：改造 `FrameParser`
- 新增 `Parse(const uint8_t*, size_t, size_t&)` 接口
- **产出**：支持原始指针输入的解析器

### 第 6 步：改造发送路径
- 所有 `SendData(id, ...)` → `conn->Send(...)`
- 广播评估是否用 `server_->Broadcast()`
- **产出**：基于 ConnectionPtr 的发送

### 第 7 步：改造关闭流程
- 区分优雅关闭和强制关闭
- 补充 `closing` 幂等保护
- **产出**：安全的连接关闭

### 第 8 步：更新业务接口
- 回调参数从 `uint64_t + ConnectionInformation` 改为 `ConnectionPtr`
- **产出**：新的业务接口

### 第 9 步：补测试
- 逐步覆盖各测试文件
- **产出**：完整的测试套件

---

## 里程碑

| 步骤 | 里程碑 | 可验证点 |
|-----|-------|---------|
| 1-2 | 基础框架 | `JsonRpcServer` 能启动并完成基础监听准备 |
| 3-5 | 消息处理 | 能完成 WebSocket 握手并处理首个数据帧 |
| 6-7 | 完整功能 | 能收发消息、正常关闭 |
| 8-9 | 完成 | 所有测试通过 |
