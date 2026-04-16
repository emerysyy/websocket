# RequestHandler 测试

## 目标

验证 JSON-RPC 请求处理正确性。

## 覆盖点

- 正常请求
- 未注册方法
- 非法 JSON
- 参数错误
- 通知无响应
- 批量请求

## 相关测试

- `test/jsonrpc_test.cc`
