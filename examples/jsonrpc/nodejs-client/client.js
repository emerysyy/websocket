#!/usr/bin/env node

/**
 * WebSocket JSON-RPC 2.0 测试客户端
 * 支持多种性能测试场景
 */

import WebSocket from 'ws';
import yargs from 'yargs';
import { hideBin } from 'yargs/helpers';

// ==================== 配置 ====================
const CONFIG = {
    host: '127.0.0.1',
    port: 9998,
    url: 'ws://127.0.0.1:9998',
};

// ==================== JSON-RPC 客户端类 ====================
class JsonRpcClient {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.requestId = 1;
        this.pendingRequests = new Map();
        this.connected = false;
        this.messageCount = 0;
        this.bytesReceived = 0;
        this.bytesSent = 0;
    }

    /**
     * 连接到服务器
     */
    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);

            this.ws.on('open', () => {
                this.connected = true;
                console.log(`✅ 连接成功: ${this.url}`);
                resolve();
            });

            this.ws.on('message', (data) => {
                this.bytesReceived += data.length;
                this.messageCount++;
                this.handleMessage(data.toString());
            });

            this.ws.on('close', () => {
                this.connected = false;
                console.log('🔌 连接已关闭');
            });

            this.ws.on('error', (error) => {
                console.error('❌ WebSocket 错误:', error.message);
                reject(error);
            });

            // 5秒超时
            setTimeout(() => {
                if (!this.connected) {
                    reject(new Error('连接超时'));
                }
            }, 5000);
        });
    }

    /**
     * 处理收到的消息
     */
    handleMessage(data) {
        try {
            const response = JSON.parse(data);

            // 处理通知（没有 id）
            if (!response.id) {
                console.log('📢 收到通知:', response.method, response.params);
                return;
            }

            // 处理响应
            const pending = this.pendingRequests.get(response.id);
            if (pending) {
                this.pendingRequests.delete(response.id);

                if (response.error) {
                    pending.reject(response.error);
                } else {
                    pending.resolve(response.result);
                }
            }
        } catch (error) {
            console.error('❌ 解析消息失败:', error.message);
        }
    }

    /**
     * 发送 JSON-RPC 请求
     */
    call(method, params = {}, timeout = 5000) {
        return new Promise((resolve, reject) => {
            if (!this.connected) {
                reject(new Error('未连接'));
                return;
            }

            const id = this.requestId++;
            const request = {
                jsonrpc: '2.0',
                method,
                params,
                id
            };

            const requestStr = JSON.stringify(request);
            this.bytesSent += requestStr.length;

            this.ws.send(requestStr);

            // 保存待处理请求
            const timer = setTimeout(() => {
                this.pendingRequests.delete(id);
                reject(new Error(`请求超时: ${method}`));
            }, timeout);

            this.pendingRequests.set(id, {
                resolve: (result) => {
                    clearTimeout(timer);
                    resolve(result);
                },
                reject: (error) => {
                    clearTimeout(timer);
                    reject(error);
                }
            });
        });
    }

    /**
     * 发送通知（不等待响应）
     */
    notify(method, params = {}) {
        if (!this.connected) {
            throw new Error('未连接');
        }

        const notification = {
            jsonrpc: '2.0',
            method,
            params
        };

        const notificationStr = JSON.stringify(notification);
        this.bytesSent += notificationStr.length;
        this.ws.send(notificationStr);
    }

    /**
     * 关闭连接
     */
    close() {
        if (this.ws) {
            this.ws.close();
        }
    }

    /**
     * 获取统计信息
     */
    getStats() {
        return {
            messageCount: this.messageCount,
            bytesReceived: this.bytesReceived,
            bytesSent: this.bytesSent,
            pendingRequests: this.pendingRequests.size
        };
    }
}

// ==================== 测试场景 ====================

/**
 * 基础功能测试
 */
async function testBasic(client) {
    console.log('\n📋 === 基础功能测试 ===\n');

    // 测试 echo
    console.log('1. 测试 echo 方法...');
    const echoResult = await client.call('echo', { message: 'Hello, WebSocket!' });
    console.log('   ✅ Echo 结果:', echoResult);

    // 测试 add
    console.log('2. 测试 add 方法...');
    const addResult = await client.call('add', { a: 42, b: 23 });
    console.log('   ✅ Add 结果:', addResult);

    // 测试 multiply
    console.log('3. 测试 multiply 方法...');
    const mulResult = await client.call('multiply', { a: 7, b: 8 });
    console.log('   ✅ Multiply 结果:', mulResult);

    // 测试 getTime
    console.log('4. 测试 getTime 方法...');
    const timeResult = await client.call('getTime');
    console.log('   ✅ Time 结果:', new Date(timeResult.timestamp * 1000).toLocaleString());

    // 测试 getConnectionCount
    console.log('5. 测试 getConnectionCount 方法...');
    const countResult = await client.call('getConnectionCount');
    console.log('   ✅ 当前连接数:', countResult.count);

    console.log('\n✅ 基础功能测试完成\n');
}

/**
 * 高频消息测试
 */
async function testStress(client) {
    console.log('\n🚀 === 高频消息测试 ===\n');

    const testCases = [
        { count: 100, label: '100 条消息', batchSize: 50 },
        { count: 1000, label: '1000 条消息', batchSize: 50 },
        { count: 5000, label: '5000 条消息', batchSize: 100 },
        { count: 10000, label: '10000 条消息', batchSize: 200 },
    ];

    for (const { count, label, batchSize } of testCases) {
        console.log(`测试: ${label}`);

        const startTime = Date.now();
        let completed = 0;

        // 分批处理，避免一次性发送太多请求导致 TCP 拥塞
        for (let i = 0; i < count; i += batchSize) {
            const batch = [];
            const currentBatchSize = Math.min(batchSize, count - i);

            // 动态超时：基础10秒 + 每批额外5秒
            const timeout = 10000 + Math.ceil(count / batchSize) * 5000;

            for (let j = 0; j < currentBatchSize; j++) {
                const idx = i + j;
                batch.push(client.call('add', { a: idx, b: idx + 1 }, timeout));
            }

            await Promise.all(batch);
            completed += currentBatchSize;

            // 显示进度（仅对大批量测试）
            if (count >= 1000 && completed % 500 === 0) {
                process.stdout.write(`   进度: ${completed}/${count}\r`);
            }
        }

        const duration = Date.now() - startTime;
        const qps = Math.round(count / (duration / 1000));

        console.log(`   ✅ ${count} 条消息耗时: ${duration}ms`);
        console.log(`   📊 QPS: ${qps} 请求/秒\n`);
    }

    console.log('✅ 高频消息测试完成\n');
}

/**
 * 大数据测试
 */
async function testLargeData(client) {
    console.log('\n📦 === 大数据测试 ===\n');

    const sizes = [
        { size: 1024, label: '1 KB' },
        { size: 10 * 1024, label: '10 KB' },
        { size: 100 * 1024, label: '100 KB' },
        { size: 1024 * 1024, label: '1 MB' },
        { size: 2 * 1024 * 1024, label: '2 MB' },
    ];

    for (const { size, label } of sizes) {
        console.log(`测试: ${label} 数据传输`);

        // 生成大数据
        const largeData = 'x'.repeat(size);

        const startTime = Date.now();
        const result = await client.call('echo', { data: largeData }, 30000);
        const duration = Date.now() - startTime;

        const isCorrect = result.data === largeData;
        console.log(`   ✅ 传输耗时: ${duration}ms`);
        console.log(`   ✅ 数据完整性: ${isCorrect ? '正确' : '错误'}`);
        console.log(`   📊 速率: ${(size / 1024 / (duration / 1000)).toFixed(2)} KB/s\n`);
    }

    console.log('✅ 大数据测试完成\n');
}

/**
 * 并发连接测试
 */
async function testConcurrent() {
    console.log('\n🔀 === 并发连接测试 ===\n');

    const clientCounts = [10, 50, 100];

    for (const count of clientCounts) {
        console.log(`测试: ${count} 个并发客户端`);

        const clients = [];
        const startTime = Date.now();

        // 创建并连接所有客户端
        for (let i = 0; i < count; i++) {
            const client = new JsonRpcClient(CONFIG.url);
            clients.push(client);
        }

        await Promise.all(clients.map(c => c.connect()));
        console.log(`   ✅ ${count} 个客户端连接成功`);

        // 每个客户端发送请求
        const requests = clients.map((client, i) =>
            client.call('add', { a: i, b: i + 1 })
        );

        await Promise.all(requests);
        const duration = Date.now() - startTime;

        console.log(`   ✅ 总耗时: ${duration}ms`);
        console.log(`   📊 平均每客户端: ${(duration / count).toFixed(2)}ms\n`);

        // 关闭所有客户端
        clients.forEach(c => c.close());

        // 等待连接关闭
        await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log('✅ 并发连接测试完成\n');
}

/**
 * 交互式模式
 */
async function interactiveMode(client) {
    console.log('\n💬 === 交互式模式 ===');
    console.log('可用命令:');
    console.log('  echo <message>      - 回显消息');
    console.log('  add <a> <b>         - 加法');
    console.log('  multiply <a> <b>    - 乘法');
    console.log('  time                - 获取服务器时间');
    console.log('  count               - 获取连接数');
    console.log('  stats               - 显示统计信息');
    console.log('  quit                - 退出');
    console.log();

    const readline = await import('readline');
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        prompt: '> '
    });

    rl.prompt();

    rl.on('line', async (line) => {
        const parts = line.trim().split(/\s+/);
        const cmd = parts[0];

        try {
            switch (cmd) {
                case 'echo':
                    const message = parts.slice(1).join(' ');
                    const echoResult = await client.call('echo', { message });
                    console.log('结果:', echoResult);
                    break;

                case 'add':
                    const a = parseInt(parts[1]);
                    const b = parseInt(parts[2]);
                    const addResult = await client.call('add', { a, b });
                    console.log('结果:', addResult);
                    break;

                case 'multiply':
                    const x = parseInt(parts[1]);
                    const y = parseInt(parts[2]);
                    const mulResult = await client.call('multiply', { a: x, b: y });
                    console.log('结果:', mulResult);
                    break;

                case 'time':
                    const timeResult = await client.call('getTime');
                    console.log('服务器时间:', new Date(timeResult.timestamp * 1000).toLocaleString());
                    break;

                case 'count':
                    const countResult = await client.call('getConnectionCount');
                    console.log('当前连接数:', countResult.count);
                    break;

                case 'stats':
                    const stats = client.getStats();
                    console.log('统计信息:', {
                        收到消息数: stats.messageCount,
                        接收字节数: stats.bytesReceived,
                        发送字节数: stats.bytesSent,
                        待处理请求: stats.pendingRequests
                    });
                    break;

                case 'quit':
                    console.log('👋 再见!');
                    client.close();
                    rl.close();
                    process.exit(0);
                    break;

                default:
                    if (cmd) {
                        console.log('未知命令:', cmd);
                    }
            }
        } catch (error) {
            console.error('❌ 错误:', error.message);
        }

        rl.prompt();
    });

    rl.on('close', () => {
        client.close();
        process.exit(0);
    });
}

// ==================== 主程序 ====================

async function main() {
    const argv = yargs(hideBin(process.argv))
        .option('test', {
            alias: 't',
            type: 'string',
            description: '测试类型: basic, stress, large, concurrent, all',
            choices: ['basic', 'stress', 'large', 'concurrent', 'all']
        })
        .option('host', {
            alias: 'h',
            type: 'string',
            default: CONFIG.host,
            description: '服务器地址'
        })
        .option('port', {
            alias: 'p',
            type: 'number',
            default: CONFIG.port,
            description: '服务器端口'
        })
        .help()
        .argv;

    // 更新配置
    CONFIG.host = argv.host;
    CONFIG.port = argv.port;
    CONFIG.url = `ws://${argv.host}:${argv.port}`;

    console.log(`
╔═══════════════════════════════════════════════╗
║  WebSocket JSON-RPC 2.0 测试客户端           ║
╚═══════════════════════════════════════════════╝
`);

    const client = new JsonRpcClient(CONFIG.url);

    try {
        await client.connect();

        if (!argv.test) {
            // 交互式模式
            await interactiveMode(client);
        } else {
            // 测试模式
            switch (argv.test) {
                case 'basic':
                    await testBasic(client);
                    break;
                case 'stress':
                    await testStress(client);
                    break;
                case 'large':
                    await testLargeData(client);
                    break;
                case 'concurrent':
                    await testConcurrent();
                    return; // concurrent 测试自己管理客户端
                case 'all':
                    await testBasic(client);
                    await testStress(client);
                    await testLargeData(client);
                    await testConcurrent();
                    break;
            }

            // 显示统计
            const stats = client.getStats();
            console.log('📊 统计信息:');
            console.log(`   收到消息: ${stats.messageCount}`);
            console.log(`   接收字节: ${(stats.bytesReceived / 1024).toFixed(2)} KB`);
            console.log(`   发送字节: ${(stats.bytesSent / 1024).toFixed(2)} KB`);

            client.close();
        }
    } catch (error) {
        console.error('❌ 错误:', error.message);
        process.exit(1);
    }
}

main();
