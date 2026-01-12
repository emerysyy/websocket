import WebSocket from 'ws';

/**
 * WebSocket Client - 单个连接管理
 */
export class WSClient {
  constructor(id, serverUrl, config) {
    this.id = id;
    this.serverUrl = serverUrl;
    this.config = config;
    this.ws = null;
    this.connected = false;
    this.connectTime = null;
    this.messageCount = 0;
    this.sendBytes = 0;
    this.recvBytes = 0;
    this.latencies = [];
    this.reconnectCount = 0;
    this.shouldReconnect = true;
    this.messageHandlers = [];
    this.requestId = 0;  // JSON-RPC 请求 ID
    this.pendingRequests = new Map();  // 待处理的请求 (id -> sendTime)
  }

  /**
   * 连接到服务器
   */
  connect() {
    return new Promise((resolve, reject) => {
      this.ws = new WebSocket(this.serverUrl, {
        handshakeTimeout: this.config.connectTimeout || 10000
      });

      this.ws.on('open', () => {
        this.connected = true;
        this.connectTime = Date.now();
        if (!this.config.quiet) {
          console.log(`[Client ${this.id}] Connected`);
        }
        resolve();
      });

      this.ws.on('message', (data, isBinary) => {
        this.recvBytes += data.length;

        // 如果是我们发送的消息的回显，计算延迟
        if (isBinary) {
          this.handleBinaryMessage(data);
        } else {
          // JSON-RPC 响应会在 handleJsonRpcResponse 中增加 messageCount
          this.handleTextMessage(data.toString());
        }

        // 触发注册的消息处理器
        this.messageHandlers.forEach(handler => handler(data, isBinary));
      });

      this.ws.on('error', (error) => {
        // 只在连接失败时输出错误,减少噪音
        if (!this.connected && !this.config.quiet) {
          console.error(`[Client ${this.id}] Connection failed: ${error.message}`);
        }
        if (!this.connected) {
          reject(error);
        }
      });

      this.ws.on('close', (code, reason) => {
        this.connected = false;

        // 只在异常断开时记录日志
        if (code !== 1000 && !this.config.quiet) {
          console.log(`[Client ${this.id}] Disconnected: ${code} - ${reason}`);
        }

        // 自动重连
        if (this.shouldReconnect && this.reconnectCount < this.config.maxReconnects) {
          this.reconnectCount++;
          const delay = this.config.reconnectDelay || 1000;
          setTimeout(() => {
            if (!this.config.quiet) {
              console.log(`[Client ${this.id}] Reconnecting... (${this.reconnectCount}/${this.config.maxReconnects})`);
            }
            this.connect().catch(err => {
              // 静默处理重连失败,避免日志洪水
            });
          }, delay);
        }
      });

      // 连接超时处理
      setTimeout(() => {
        if (!this.connected) {
          this.ws.terminate();
          reject(new Error('Connection timeout'));
        }
      }, this.config.connectTimeout || 10000);
    });
  }

  /**
   * 处理二进制消息（假设包含时间戳用于延迟计算）
   */
  handleBinaryMessage(data) {
    if (data.length >= 8) {
      // 读取发送时间戳（假设前8字节是timestamp）
      const sendTime = data.readBigUInt64BE(0);
      const recvTime = BigInt(Date.now());
      const latency = Number(recvTime - sendTime);
      this.latencies.push(latency);

      // 只保留最近1000个延迟样本
      if (this.latencies.length > 1000) {
        this.latencies.shift();
      }
    }
  }

  /**
   * 处理 JSON-RPC 响应消息
   */
  handleJsonRpcResponse(text) {
    try {
      const msg = JSON.parse(text);

      // 检查是否是我们发送的请求的响应
      if (msg.id && this.pendingRequests.has(msg.id)) {
        const sendTime = this.pendingRequests.get(msg.id);
        const latency = Date.now() - sendTime;

        this.latencies.push(latency);
        if (this.latencies.length > 1000) {
          this.latencies.shift();
        }

        // 删除已完成的请求
        this.pendingRequests.delete(msg.id);

        // 增加响应计数
        this.messageCount++;
      }
    } catch (e) {
      // 忽略解析错误
    }
  }

  /**
   * 处理文本消息
   */
  handleTextMessage(text) {
    // 使用 JSON-RPC 响应处理
    this.handleJsonRpcResponse(text);
  }

  /**
   * 发送文本消息
   */
  sendText(message) {
    if (!this.connected || !this.ws) {
      return false;
    }

    try {
      const data = typeof message === 'string' ? message : JSON.stringify(message);
      this.ws.send(data);
      this.sendBytes += data.length;
      return true;
    } catch (error) {
      console.error(`[Client ${this.id}] Send error: ${error.message}`);
      return false;
    }
  }

  /**
   * 发送二进制消息（包含时间戳用于延迟测试）
   */
  sendBinary(payload) {
    if (!this.connected || !this.ws) {
      return false;
    }

    try {
      const buffer = Buffer.allocUnsafe(8 + payload.length);
      buffer.writeBigUInt64BE(BigInt(Date.now()), 0); // 写入时间戳
      Buffer.from(payload).copy(buffer, 8);

      this.ws.send(buffer);
      this.sendBytes += buffer.length;
      return true;
    } catch (error) {
      console.error(`[Client ${this.id}] Send error: ${error.message}`);
      return false;
    }
  }

  /**
   * 发送随机数据
   */
  sendRandom(size = 1024) {
    const payload = Buffer.alloc(size);
    for (let i = 0; i < size; i++) {
      payload[i] = Math.floor(Math.random() * 256);
    }
    return this.sendBinary(payload);
  }

  /**
   * 发送 JSON-RPC Echo 请求
   */
  sendEcho(data) {
    if (!this.connected || !this.ws) {
      return false;
    }

    try {
      const requestId = `${this.id}-${++this.requestId}`;
      const request = {
        jsonrpc: '2.0',
        id: requestId,
        method: 'echo',
        params: data || {
          message: `Hello from client ${this.id}`,
          timestamp: Date.now(),
          random: Math.random()
        }
      };

      const jsonStr = JSON.stringify(request);
      this.ws.send(jsonStr);
      this.sendBytes += jsonStr.length;

      // 记录请求时间用于延迟计算
      this.pendingRequests.set(requestId, Date.now());

      return true;
    } catch (error) {
      console.error(`[Client ${this.id}] Send error: ${error.message}`);
      return false;
    }
  }

  /**
   * 注册消息处理器
   */
  onMessage(handler) {
    this.messageHandlers.push(handler);
  }

  /**
   * 断开连接
   */
  disconnect() {
    this.shouldReconnect = false;
    if (this.ws) {
      this.ws.close();
    }
  }

  /**
   * 获取统计信息
   */
  getStats() {
    const sortedLatencies = [...this.latencies].sort((a, b) => a - b);

    let avgLatency = 0;
    let minLatency = 0;
    let maxLatency = 0;
    let p50 = 0;
    let p95 = 0;
    let p99 = 0;

    if (sortedLatencies.length > 0) {
      const sum = sortedLatencies.reduce((a, b) => a + b, 0);
      avgLatency = sum / sortedLatencies.length;
      minLatency = sortedLatencies[0];
      maxLatency = sortedLatencies[sortedLatencies.length - 1];
      p50 = sortedLatencies[Math.floor(sortedLatencies.length * 0.5)];
      p95 = sortedLatencies[Math.floor(sortedLatencies.length * 0.95)];
      p99 = sortedLatencies[Math.floor(sortedLatencies.length * 0.99)];
    }

    return {
      id: this.id,
      connected: this.connected,
      messageCount: this.messageCount,
      sendBytes: this.sendBytes,
      recvBytes: this.recvBytes,
      reconnectCount: this.reconnectCount,
      latency: {
        avg: Math.round(avgLatency),
        min: Math.round(minLatency),
        max: Math.round(maxLatency),
        p50: Math.round(p50),
        p95: Math.round(p95),
        p99: Math.round(p99)
      },
      uptime: this.connectTime ? Date.now() - this.connectTime : 0
    };
  }
}
