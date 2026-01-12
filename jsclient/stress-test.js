#!/usr/bin/env node

import { StressTest } from './StressTest.js';

/**
 * 命令行参数解析
 */
function parseArgs() {
  const args = process.argv.slice(2);
  const config = {};

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    const nextArg = args[i + 1];

    switch (arg) {
      case '-h':
      case '--help':
        printHelp();
        process.exit(0);
        break;

      case '-s':
      case '--server':
        if (nextArg) {
          config.serverUrl = nextArg;
          i++;
        }
        break;

      case '-c':
      case '--clients':
        if (nextArg) {
          config.totalClients = parseInt(nextArg, 10);
          i++;
        }
        break;

      case '-n':
      case '--concurrent':
        if (nextArg) {
          config.concurrentConnections = parseInt(nextArg, 10);
          i++;
        }
        break;

      case '-d':
      case '--duration':
        if (nextArg) {
          config.testDuration = parseInt(nextArg, 10);
          i++;
        }
        break;

      case '-m':
      case '--message-size':
        if (nextArg) {
          config.messageSize = parseInt(nextArg, 10);
          i++;
        }
        break;

      case '-i':
      case '--interval':
        if (nextArg) {
          config.messageInterval = parseInt(nextArg, 10);
          i++;
        }
        break;

      case '-w':
      case '--warmup':
        if (nextArg) {
          config.warmupTime = parseInt(nextArg, 10);
          i++;
        }
        break;

      case '--stats-interval':
        if (nextArg) {
          config.statsInterval = parseInt(nextArg, 10);
          i++;
        }
        break;

      case '-q':
      case '--quiet':
        config.quiet = true;
        break;

      case '-v':
      case '--verbose':
        config.quiet = false;
        break;

      default:
        console.error(`Unknown option: ${arg}`);
        console.error('Use --help for usage information');
        process.exit(1);
    }
  }

  return config;
}

/**
 * 打印帮助信息
 */
function printHelp() {
  console.log(`
DarwinCore WebSocket JSON-RPC Stress Test Client

Usage: node stress-test.js [options]

Options:
  -s, --server <url>           Server URL (default: ws://localhost:8080)
  -c, --clients <number>       Total number of clients (default: 100)
  -n, --concurrent <number>    Concurrent connections (default: 10)
  -d, --duration <seconds>     Test duration in seconds (default: 60)
  -m, --message-size <bytes>   Message size in bytes (default: 1024)
  -i, --interval <ms>          Message send interval in ms (default: 100)
  -w, --warmup <seconds>       Warmup time in seconds (default: 5)
  --stats-interval <ms>        Stats report interval in ms (default: 5000)
  -q, --quiet                  Quiet mode (default: enabled)
  -v, --verbose                Verbose mode (show all logs)
  -h, --help                   Show this help message

Examples:
  # Default test (100 clients, 60 seconds, 100ms interval)
  node stress-test.js

  # Heavy load (1000 clients, 5 minutes)
  node stress-test.js -c 1000 -d 300

  # Extreme load (fast messaging)
  node stress-test.js -c 500 -i 50 -m 512

  # Verbose mode (show all connection logs)
  node stress-test.js -v -c 100

  # Custom server
  node stress-test.js -s ws://192.168.1.100:9000

Note:
  This stress test sends JSON-RPC 'echo' requests and measures round-trip latency.
  The server must support the JSON-RPC 'echo' method.

  Default is quiet mode - only shows statistics and errors.
  Use -v for verbose output to see individual client connections.
`);
}

/**
 * 主函数
 */
async function main() {
  // 解析命令行参数
  const userConfig = parseArgs();

  // 创建压测实例
  const stressTest = new StressTest(userConfig);

  // 处理中断信号
  process.on('SIGINT', async () => {
    console.log('\n\nReceived SIGINT, stopping test...');
    await stressTest.stop();
    process.exit(0);
  });

  process.on('SIGTERM', async () => {
    console.log('\n\nReceived SIGTERM, stopping test...');
    await stressTest.stop();
    process.exit(0);
  });

  // 启动压测
  try {
    await stressTest.start();
  } catch (error) {
    console.error('Test failed:', error);
    process.exit(1);
  }
}

// 运行主函数
main();
