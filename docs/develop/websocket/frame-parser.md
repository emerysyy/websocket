# FrameParser

## 目标

把原始字节流解析成 WebSocket 帧。

## 公开接口

- `Parse(const std::vector<uint8_t>&, size_t&)`
- `Parse(const uint8_t*, size_t, size_t&)` 计划新增
- `IsComplete(...)`

## 关键点

- 支持 continuation / text / binary / close / ping / pong
- 处理 7 位、16 位、64 位长度编码
- 处理客户端掩码
- 支持分片到达
- 解析失败时不破坏后续数据

## 相关实现

- `src/frame_parser.cc`
- `include/darwincore/websocket/frame_parser.h`

## 验收

- 最小帧可解析
- 掩码帧可解码
- 分片数据可连续解析
- 长帧长度编码正确
