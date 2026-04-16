# nodejs-client

## 目标

Node.js 实现的 WebSocket JSON-RPC 客户端，支持多种测试场景。

## 相关文件

- `examples/nodejs-client/`

## 依赖

- `ws` - WebSocket 客户端库
- `yargs` - 命令行参数解析

## 安装

```bash
cd examples/nodejs-client
npm install
```

## 运行方式

### 交互式模式

```bash
npm start
# 或
node client.js
```

可用命令：
- `echo <message>` - 回显消息
- `add <a> <b>` - 加法
- `multiply <a> <b>` - 乘法
- `time` - 获取时间
- `count` - 获取连接数
- `stats` - 显示统计
- `quit` - 退出

### 自动化测试

| 命令 | 说明 |
|------|------|
| `npm run test:echo` | Echo 测试 |
| `npm run test:stress` | 高频消息测试（100/1000/5000 条） |
| `npm run test:large` | 大数据传输测试（1KB/10KB/100KB/1MB） |
| `npm run test:concurrent` | 并发连接测试（10/50/100 客户端） |
| `node client.js --test all` | 运行全部测试 |

### 自定义服务器

```bash
node client.js --host 127.0.0.1 --port 9998 --test basic
```

## 测试输出示例

### 高频测试

```
🚀 === 高频消息测试 ===
测试: 1000 条消息
   ✅ 耗时: 89ms
   📊 QPS: 11235 请求/秒
```

### 大数据传输

```
📦 === 大数据测试 ===
测试: 1 MB 数据传输
   ✅ 耗时: 125ms
   ✅ 数据完整性: 正确
   📊 速率: 8192.00 KB/s
```

## 验收

- 能连接到服务器
- 交互式命令正常
- 自动化测试正常输出
- QPS 和传输速率统计正确
- 并发测试正常