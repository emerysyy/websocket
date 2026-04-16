# FrameBuilder

## 目标

把业务数据编码成 WebSocket 帧。

## 公开接口

- `CreateTextFrame(...)`
- `CreateBinaryFrame(...)`
- `CreatePingFrame(...)`
- `CreatePongFrame(...)`
- `CreateCloseFrame(...)`
- `BuildFrame(...)`

## 关键点

- 服务器发送帧不加掩码
- 正确编码长度字段
- Close 帧支持 code 和 reason

## 相关实现

- `src/frame_builder.cc`
- `include/darwincore/websocket/frame_builder.h`

## 验收

- 文本帧格式正确
- 二进制帧格式正确
- Ping/Pong/Close 帧格式正确
- 长度边界正确
