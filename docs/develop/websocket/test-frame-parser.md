# FrameParser 测试

## 目标

验证帧解析、掩码、分片和长度编码。

## 覆盖点

- 最小文本帧
- 掩码帧
- 不完整帧
- 扩展长度帧
- 分片到达
- 非法 opcode

## 相关测试

- `test/frame_parser_test.cc`
