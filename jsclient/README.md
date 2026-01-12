# DarwinCore WebSocket Stress Test Client

高性能 Node.js WebSocket 压力测试客户端，用于对 DarwinCore 服务器进行并发压力测试。

## 功能特性

- **高并发连接**：支持同时创建数千个 WebSocket 连接
- **灵活配置**：通过命令行参数自定义测试参数
- **延迟统计**：实时计算平均延迟、P50、P95、P99 延迟
- **吞吐量测试**：测量消息收发速率和网络吞吐量
- **自动重连**：连接断开时自动尝试重连
- **实时统计**：测试过程中实时输出统计信息
- **详细报告**：测试结束后生成完整的性能报告

## 安装

```bash
cd jsclient
npm install
```

## 使用方法

### 基本用法

```bash
# 使用默认配置运行（100个客户端，60秒测试）
npm start

# 或直接运行
node stress-test.js
```

### 自定义参数

```bash
# 指定服务器地址
node stress-test.js -s ws://192.168.1.100:8080

# 设置客户端数量
node stress-test.js -c 500

# 设置测试时长（秒）
node stress-test.js -d 300

# 高频小消息测试
node stress-test.js -m 128 -i 100

# 完整配置示例
node stress-test.js \
  -s ws://localhost:8080 \
  -c 1000 \
  -n 50 \
  -d 120 \
  -m 512 \
  -i 500 \
  -w 10
```

### 命令行参数

| 参数 | 简写 | 说明 | 默认值 |
|------|------|------|--------|
| `--server <url>` | `-s` | WebSocket 服务器地址 | `ws://localhost:8080` |
| `--clients <number>` | `-c` | 总客户端数量 | `100` |
| `--concurrent <number>` | `-n` | 并发连接数 | `10` |
| `--duration <seconds>` | `-d` | 测试时长（秒） | `60` |
| `--message-size <bytes>` | `-m` | 消息大小（字节） | `1024` |
| `--interval <ms>` | `-i` | 消息发送间隔（毫秒） | `1000` |
| `--warmup <seconds>` | `-w` | 预热时间（秒） | `5` |
| `--stats-interval <ms>` | - | 统计输出间隔（毫秒） | `5000` |
| `--help` | `-h` | 显示帮助信息 | - |

## 测试场景

### 场景1：连接数压力测试

测试服务器能承受的最大并发连接数：

```bash
# 逐步增加连接数
node stress-test.js -c 100 -d 30
node stress-test.js -c 500 -d 30
node stress-test.js -c 1000 -d 30
node stress-test.js -c 5000 -d 30
```

### 场景2：消息吞吐量测试

高频发送小消息：

```bash
# 每秒发送10条消息，128字节
node stress-test.js -c 100 -i 100 -m 128

# 每秒发送100条消息，64字节
node stress-test.js -c 100 -i 10 -m 64
```

### 场景3：大文件传输测试

大消息传输：

```bash
# 4KB 消息
node stress-test.js -m 4096

# 64KB 消息
node stress-test.js -m 65536

# 1MB 消息
node stress-test.js -m 1048576
```

### 场景4：长时间稳定性测试

长时间运行检测内存泄漏：

```bash
# 运行1小时
node stress-test.js -c 500 -d 3600
```

## 输出示例

```
============================================================
DarwinCore WebSocket Stress Test
============================================================
Server: ws://localhost:8080
Total Clients: 100
Concurrent Connections: 10
Test Duration: 60s
Message Size: 1024 bytes
Message Interval: 1000ms
============================================================

Phase 1: Warming up with 10 connections...
[Client 1] Connected
[Client 2] Connected
...
Connected: 10/10 clients
Warmup for 5 seconds...

Phase 2: Connecting all 100 clients...
Connected: 10/20 clients
Connected: 10/30 clients
...
Connected: 10/100 clients

Phase 3: Stress testing for 60 seconds...

------------------------------------------------------------
[15.2s] Statistics:
  Connected: 98/100
  Total Messages: 1456
  Messages/sec: 95
  Sent: 1.42 MB (93.12 KB/s)
  Received: 1.38 MB
  Latency (ms): avg=12 p50=11 p95=18 p99=23
  Reconnects: 2
------------------------------------------------------------

...

============================================================
FINAL REPORT
============================================================
Duration: 60.00s
Total Clients: 100
Final Connected: 98
Total Messages: 5834
Average Messages/sec: 97
Total Sent: 5.70 MB
Total Received: 5.65 MB
Throughput: 95.12 KB/s
Total Reconnects: 2

Latency Statistics (ms):
  Average: 12
  P50: 11
  P95: 18
  P99: 23
============================================================
```

## 架构说明

### 文件结构

- `stress-test.js` - 主程序入口，命令行参数解析
- `StressTest.js` - 压力测试管理器，协调整体测试流程
- `WSClient.js` - 单个 WebSocket 客户端实现

### 测试流程

1. **预热阶段**：连接少量客户端，让服务器稳定
2. **连接阶段**：逐步连接所有客户端（分批并发连接）
3. **压测阶段**：定期发送消息，收集统计数据
4. **报告阶段**：生成完整的性能报告

### 延迟计算

客户端在发送的二进制消息中携带时间戳（前8字节），服务器回显后，客户端计算往返延迟（RTT）。

## 注意事项

1. **系统限制**：默认情况下，操作系统对文件描述符数量有限制。在测试大量连接时，需要调整限制：

   ```bash
   # macOS/Linux
   ulimit -n 10000
   ```

2. **服务器配置**：确保 DarwinCore 服务器已配置足够的 Reactor 线程和 Worker 线程。

3. **网络环境**：客户端和服务器应在同一局域网内测试，避免网络带宽成为瓶颈。

4. **资源监控**：建议同时监控服务器端资源使用情况（CPU、内存、网络）。

## 性能调优

- 增加 `--concurrent` 参数可以加快连接建立速度
- 减小 `--interval` 可以提高消息发送频率
- 调整 `--message-size` 测试不同负载下的性能
- 使用 `--warmup` 让服务器进入稳定状态后再开始测试

## 故障排查

### 连接失败

- 检查服务器是否启动
- 检查服务器地址和端口是否正确
- 检查防火墙设置

### 延迟过高

- 检查网络延迟（ping 测试）
- 检查服务器 CPU 使用率
- 减少客户端数量或消息频率

### 连接断开

- 检查服务器日志
- 检查服务器连接超时设置
- 增加重连次数和延迟
