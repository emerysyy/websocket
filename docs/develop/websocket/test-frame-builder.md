# FrameBuilder 测试

## 目标

验证帧构建输出符合 RFC 6455。

## 覆盖点

- 文本帧
- 二进制帧
- Ping/Pong 帧
- Close 帧
- 125/126/127 长度边界

## 相关测试

- `test/frame_builder_test.cc`
