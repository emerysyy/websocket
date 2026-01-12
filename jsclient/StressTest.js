import { WSClient } from './WSClient.js';

/**
 * 压力测试管理器
 */
export class StressTest {
  constructor(config) {
    this.config = {
      serverUrl: 'ws://localhost:8080',
      totalClients: 100,          // 总客户端数量
      concurrentConnections: 50,   // 并发连接数 - 优化：降低并发避免服务端过载
      testDuration: 60,            // 测试时长（秒）
      messageInterval: 100,        // 消息发送间隔（毫秒）- 优化：更快的发送频率
      messageSize: 1024,           // 消息大小（字节）
      warmupTime: 5,               // 预热时间（秒）
      connectTimeout: 30000,       // 连接超时（毫秒）- 优化：增加超时时间
      maxReconnects: 5,            // 最大重连次数 - 优化：增加重试次数
      reconnectDelay: 1000,        // 重连延迟（毫秒）
      statsInterval: 5000,         // 统计输出间隔（毫秒）
      quiet: true,                 // 安静模式 - 减少连接日志
      ...config
    };

    this.clients = [];
    this.startTime = null;
    this.endTime = null;
    this.running = false;
    this.statsTimer = null;
    this.messageTimer = null;
  }

  /**
   * 启动压力测试
   */
  async start() {
    console.log('='.repeat(60));
    console.log('DarwinCore WebSocket Stress Test');
    console.log('='.repeat(60));
    console.log(`Server: ${this.config.serverUrl}`);
    console.log(`Total Clients: ${this.config.totalClients}`);
    console.log(`Concurrent Connections: ${this.config.concurrentConnections}`);
    console.log(`Test Duration: ${this.config.testDuration}s`);
    console.log(`Message Size: ${this.config.messageSize} bytes`);
    console.log(`Message Interval: ${this.config.messageInterval}ms`);
    console.log('='.repeat(60));
    console.log();

    this.running = true;
    this.startTime = Date.now();

    // 阶段1: 预热连接
    console.log(`Phase 1: Warming up with ${this.config.concurrentConnections} connections...`);
    await this.connectClients(this.config.concurrentConnections);

    console.log(`Warmup for ${this.config.warmupTime} seconds...`);
    await this.sleep(this.config.warmupTime * 1000);

    // 阶段2: 完整连接
    console.log(`\nPhase 2: Connecting all ${this.config.totalClients} clients...`);
    await this.connectClients(this.config.totalClients - this.config.concurrentConnections);

    // 阶段3: 开始压测
    console.log(`\nPhase 3: Stress testing for ${this.config.testDuration} seconds...`);
    this.startPeriodicMessaging();
    this.startStatsReporting();

    // 等待测试完成
    await this.sleep(this.config.testDuration * 1000);

    // 停止测试
    await this.stop();
  }

  /**
   * 批量连接客户端
   */
  async connectClients(count) {
    const batchSize = this.config.concurrentConnections;
    let connected = 0;
    const quiet = this.config.quiet;

    for (let i = 0; i < count; i += batchSize) {
      const batch = Math.min(batchSize, count - i);
      const promises = [];

      for (let j = 0; j < batch; j++) {
        const clientId = this.clients.length + 1;
        const client = new WSClient(clientId, this.config.serverUrl, {
          ...this.config,
          quiet  // 传递安静模式配置
        });

        // 注册消息处理器
        client.onMessage((data, isBinary) => {
          // 可以在这里添加自定义消息处理逻辑
        });

        this.clients.push(client);
        promises.push(client.connect().catch(err => {
          if (!quiet) {
            console.error(`Failed to connect client ${clientId}: ${err.message}`);
          }
        }));
      }

      await Promise.all(promises);
      connected += batch;

      // 只在关键节点输出进度
      if (!quiet || connected % 100 === 0 || connected === count) {
        console.log(`Connected: ${connected}/${this.clients.length} clients`);
      }

      // 避免连接过快 - 给服务端更多时间处理握手
      await this.sleep(500);
    }

    // 等待所有连接稳定
    await this.sleep(2000);
  }

  /**
   * 启动定期消息发送
   */
  startPeriodicMessaging() {
    this.messageTimer = setInterval(() => {
      if (!this.running) return;

      const connectedClients = this.clients.filter(c => c.connected);
      const activeClients = connectedClients.length;

      // 每轮让所有客户端都发送 JSON-RPC Echo 请求（真正压测）
      const sendRate = 1.0; // 100%的客户端每轮发送
      const senderCount = Math.ceil(activeClients * sendRate);

      for (let i = 0; i < senderCount; i++) {
        const client = connectedClients[Math.floor(Math.random() * activeClients)];
        if (client) {
          // 发送 JSON-RPC Echo 请求
          client.sendEcho({
            client: client.id,
            timestamp: Date.now(),
            random: Math.random()
          });
        }
      }
    }, this.config.messageInterval);
  }

  /**
   * 启动统计报告
   */
  startStatsReporting() {
    this.statsTimer = setInterval(() => {
      if (!this.running) return;
      this.printStats();
    }, this.config.statsInterval);
  }

  /**
   * 打印统计信息
   */
  printStats() {
    const stats = this.getAggregatedStats();
    const elapsed = ((Date.now() - this.startTime) / 1000).toFixed(1);

    console.log('\n' + '-'.repeat(60));
    console.log(`[${elapsed}s] Statistics:`);
    console.log(`  Connected: ${stats.connectedClients}/${this.clients.length}`);
    console.log(`  Total Messages: ${stats.totalMessages}`);
    console.log(`  Messages/sec: ${stats.messagesPerSec}`);
    console.log(`  Sent: ${this.formatBytes(stats.totalSent)} (${this.formatBytes(stats.bytesPerSec)}/s)`);
    console.log(`  Received: ${this.formatBytes(stats.totalRecv)}`);
    console.log(`  Latency (ms): avg=${stats.avgLatency} p50=${stats.p50} p95=${stats.p95} p99=${stats.p99}`);
    console.log(`  Reconnects: ${stats.totalReconnects}`);
    console.log('-'.repeat(60));
  }

  /**
   * 获取聚合统计信息
   */
  getAggregatedStats() {
    let totalMessages = 0;
    let totalSent = 0;
    let totalRecv = 0;
    let totalReconnects = 0;
    let connectedClients = 0;
    let allLatencies = [];

    this.clients.forEach(client => {
      const stats = client.getStats();
      totalMessages += stats.messageCount;
      totalSent += stats.sendBytes;
      totalRecv += stats.recvBytes;
      totalReconnects += stats.reconnectCount;
      if (stats.connected) connectedClients++;

      // 收集所有延迟数据
      stats.latency ? allLatencies.push(stats.latency.avg) : null;
    });

    const elapsed = (Date.now() - this.startTime) / 1000;
    const sortedLatencies = allLatencies.sort((a, b) => a - b);

    let avgLatency = 0;
    let p50 = 0;
    let p95 = 0;
    let p99 = 0;

    if (sortedLatencies.length > 0) {
      avgLatency = sortedLatencies.reduce((a, b) => a + b, 0) / sortedLatencies.length;
      p50 = sortedLatencies[Math.floor(sortedLatencies.length * 0.5)];
      p95 = sortedLatencies[Math.floor(sortedLatencies.length * 0.95)];
      p99 = sortedLatencies[Math.floor(sortedLatencies.length * 0.99)];
    }

    return {
      connectedClients,
      totalMessages,
      messagesPerSec: Math.round(totalMessages / elapsed),
      totalSent,
      totalRecv,
      bytesPerSec: Math.round(totalSent / elapsed),
      totalReconnects,
      avgLatency: Math.round(avgLatency),
      p50: Math.round(p50),
      p95: Math.round(p95),
      p99: Math.round(p99)
    };
  }

  /**
   * 停止压力测试
   */
  async stop() {
    console.log('\n' + '='.repeat(60));
    console.log('Stopping stress test...');
    this.running = false;

    if (this.messageTimer) {
      clearInterval(this.messageTimer);
    }
    if (this.statsTimer) {
      clearInterval(this.statsTimer);
    }

    // 断开所有连接
    await Promise.all(this.clients.map(client => client.disconnect()));

    this.endTime = Date.now();
    const duration = ((this.endTime - this.startTime) / 1000).toFixed(2);

    console.log(`\nTest completed in ${duration}s`);

    // 打印最终统计
    this.printFinalReport();

    console.log('='.repeat(60));
  }

  /**
   * 打印最终报告
   */
  printFinalReport() {
    const stats = this.getAggregatedStats();
    const duration = (this.endTime - this.startTime) / 1000;

    console.log('\n' + '='.repeat(60));
    console.log('FINAL REPORT');
    console.log('='.repeat(60));
    console.log(`Duration: ${duration.toFixed(2)}s`);
    console.log(`Total Clients: ${this.clients.length}`);
    console.log(`Final Connected: ${stats.connectedClients}`);
    console.log(`Total Messages: ${stats.totalMessages}`);
    console.log(`Average Messages/sec: ${Math.round(stats.totalMessages / duration)}`);
    console.log(`Total Sent: ${this.formatBytes(stats.totalSent)}`);
    console.log(`Total Received: ${this.formatBytes(stats.totalRecv)}`);
    console.log(`Throughput: ${this.formatBytes(stats.totalSent / duration)}/s`);
    console.log(`Total Reconnects: ${stats.totalReconnects}`);
    console.log('\nLatency Statistics (ms):');
    console.log(`  Average: ${stats.avgLatency}`);
    console.log(`  P50: ${stats.p50}`);
    console.log(`  P95: ${stats.p95}`);
    console.log(`  P99: ${stats.p99}`);
    console.log('='.repeat(60));
  }

  /**
   * 格式化字节数
   */
  formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  }

  /**
   * 休眠函数
   */
  sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}
