# JsonRpcServer 测试

## 目标

验证服务器封装层与连接生命周期协作正确。

## 覆盖点

- 启动 / 停止
- 连接回调
- 广播
- 关闭流程
- 连接断开后不再发送

## 相关测试

- 新增 `test/jsonrpc_server_test.cc`
