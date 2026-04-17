// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/frame_builder.h"

#include <cstring>

namespace darwincore {
namespace websocket {

std::vector<uint8_t> FrameBuilder::CreateTextFrame(const std::string& message) {
  std::vector<uint8_t> data(message.begin(), message.end());
  return BuildFrame(OpCode::kText, data);
}

std::vector<uint8_t> FrameBuilder::CreateBinaryFrame(
    const std::vector<uint8_t>& data) {
  return BuildFrame(OpCode::kBinary, data);
}

std::vector<uint8_t> FrameBuilder::CreatePingFrame(
    const std::vector<uint8_t>& payload) {
  // RFC6455: 控制帧载荷必须 <= 125 字节
  std::vector<uint8_t> trimmed_payload = payload;
  if (trimmed_payload.size() > 125) {
    trimmed_payload.resize(125);
  }
  return BuildFrame(OpCode::kPing, trimmed_payload);
}

std::vector<uint8_t> FrameBuilder::CreatePongFrame(
    const std::vector<uint8_t>& payload) {
  // RFC6455: 控制帧载荷必须 <= 125 字节
  std::vector<uint8_t> trimmed_payload = payload;
  if (trimmed_payload.size() > 125) {
    trimmed_payload.resize(125);
  }
  return BuildFrame(OpCode::kPong, trimmed_payload);
}

std::vector<uint8_t> FrameBuilder::CreateCloseFrame(uint16_t code,
                                                    const std::string& reason) {
  std::vector<uint8_t> payload;
  payload.reserve(2 + reason.size());

  // 写入关闭码（大端序）
  payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
  payload.push_back(static_cast<uint8_t>(code & 0xFF));

  // 写入原因（RFC6455: 控制帧载荷必须 <= 125 字节，2 字节 code + reason <= 123）
  constexpr size_t kMaxReasonLength = 123;
  if (!reason.empty()) {
    auto end = reason.end();
    if (reason.size() > kMaxReasonLength) {
      end = reason.begin() + kMaxReasonLength;
    }
    payload.insert(payload.end(), reason.begin(), end);
  }

  return BuildFrame(OpCode::kClose, payload);
}

std::vector<uint8_t> FrameBuilder::BuildFrame(OpCode opcode,
                                              const std::vector<uint8_t>& payload,
                                              bool fin) {
  std::vector<uint8_t> frame;
  frame.reserve(2 + payload.size());

  // 第一个字节：FIN + opcode
  uint8_t byte0 = static_cast<uint8_t>(opcode);
  if (fin) {
    byte0 |= 0x80;  // 设置 FIN 位
  }
  frame.push_back(byte0);

  // 第二个字节：载荷长度（服务器发送不加掩码）
  EncodeLength(frame, payload.size());

  // 载荷数据
  if (!payload.empty()) {
    frame.insert(frame.end(), payload.begin(), payload.end());
  }

  return frame;
}

void FrameBuilder::EncodeLength(std::vector<uint8_t>& frame, uint64_t length) {
  if (length <= 125) {
    frame.push_back(static_cast<uint8_t>(length));
  } else if (length <= 0xFFFF) {
    frame.push_back(126);
    frame.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(length & 0xFF));
  } else {
    frame.push_back(127);
    // 8 字节长度（大端序）
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<uint8_t>((length >> (i * 8)) & 0xFF));
    }
  }
}

}  // namespace websocket
}  // namespace darwincore
