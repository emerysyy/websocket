# 附录：DarwinCore 头文件清单

## 完整目录结构

```
3rd/include/darwincore/
├── log/
│   └── (日志相关)
├── network/
│   ├── base/
│   │   ├── channel.h
│   │   ├── event_loop.h
│   │   ├── event_loop_group.h
│   │   ├── event_loop_thread.h
│   │   ├── kqueue_poller.h
│   │   ├── poller.h
│   │   ├── timer.h
│   │   ├── timer_queue.h
│   │   └── timestamp.h
│   ├── codec/         # 编解码器
│   ├── gateway/       # 网关组件
│   ├── proxy/         # 代理组件
│   ├── transport/     # 传输层
│   │   ├── acceptor.h
│   │   ├── buffer.h
│   │   ├── connection.h
│   │   ├── connection_pool.h
│   │   ├── connector.h
│   │   └── server.h
│   ├── configuration.h
│   └── logger.h
├── runtime/           # 运行时组件
├── storage/           # 存储组件
└── system/            # 系统组件
```

---

## 关键头文件路径汇总

| 组件 | 头文件路径 |
|------|-----------|
| EventLoopGroup | `darwincore/network/base/event_loop_group.h` |
| Server | `darwincore/network/transport/server.h` |
| Connection | `darwincore/network/transport/connection.h` |
| Buffer | `darwincore/network/transport/buffer.h` |
| Timestamp | `darwincore/network/base/timestamp.h` |
