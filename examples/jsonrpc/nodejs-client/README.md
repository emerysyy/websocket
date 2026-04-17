# WebSocket JSON-RPC 2.0 测试客户端

Node.js 实现的 WebSocket JSON-RPC 客户端，支持多种性能测试场景。

## 功能特性

- ✅ 完整的 JSON-RPC 2.0 协议支持
- ✅ 高频消息测试（QPS 测试）
- ✅ 大数据传输测试（1KB - 1MB）
- ✅ 并发连接测试（10 - 100 客户端）
- ✅ 交互式命令行模式
- ✅ 详细的性能统计

## 安装

```bash
cd examples/nodejs-client
npm install
```

## 使用方法

### 1. 交互式模式

```bash
npm start
```

可用命令：
- `echo <message>` - 回显消息
- `add <a> <b>` - 加法运算
- `multiply <a> <b>` - 乘法运算
- `time` - 获取服务器时间
- `count` - 获取当前连接数
- `stats` - 显示统计信息
- `quit` - 退出

### 2. 基础功能测试

```bash
npm run test:echo
# 或
node client.js --test basic
```

测试所有基础功能：echo, add, multiply, getTime, getConnectionCount

### 3. 高频消息测试

```bash
npm run test:stress
# 或
node client.js --test stress
```

测试场景：
- 100 条消息
- 1000 条消息
- 5000 条消息

输出 QPS（每秒请求数）统计。

### 4. 大数据传输测试

```bash
npm run test:large
# 或
node client.js --test large
```

测试场景：
- 1 KB 数据
- 10 KB 数据
- 100 KB 数据
- 1 MB 数据

验证数据完整性和传输速率。

### 5. 并发连接测试

```bash
npm run test:concurrent
# 或
node client.js --test concurrent
```

测试场景：
- 10 个并发客户端
- 50 个并发客户端
- 100 个并发客户端

### 6. 完整测试

```bash
node client.js --test all
```

运行所有测试场景。

### 7. 自定义服务器地址

```bash
node client.js --host 192.168.1.100 --port 8080 --test basic
```

## 示例输出

### 高频消息测试
```
🚀 === 高频消息测试 ===

测试: 100 条消息
   ✅ 100 条消息耗时: 15ms
   📊 QPS: 6666 请求/秒

测试: 1000 条消息
   ✅ 1000 条消息耗时: 89ms
   📊 QPS: 11235 请求/秒

测试: 5000 条消息
   ✅ 5000 条消息耗时: 423ms
   📊 QPS: 11820 请求/秒
```

### 大数据测试
```
📦 === 大数据测试 ===

测试: 1 KB 数据传输
   ✅ 传输耗时: 3ms
   ✅ 数据完整性: 正确
   📊 速率: 341.33 KB/s

测试: 1 MB 数据传输
   ✅ 传输耗时: 125ms
   ✅ 数据完整性: 正确
   📊 速率: 8192.00 KB/s
```

## 性能基准

在 MacBook Pro (M1, 16GB) 上的测试结果：

| 测试类型 | 指标 | 结果 |
|---------|------|------|
| 高频消息 | QPS | ~12,000 请求/秒 |
| 大数据传输 | 1MB | ~8 MB/s |
| 并发连接 | 100客户端 | ~200ms |

## 依赖

- `ws` - WebSocket 客户端库
- `yargs` - 命令行参数解析

## 许可证

MIT
