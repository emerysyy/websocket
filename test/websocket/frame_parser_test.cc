// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/frame_parser.h"
#include "darwincore/websocket/frame_builder.h"

namespace {

using namespace darwincore::websocket;

// 创建文本帧
std::vector<uint8_t> CreateTextFrame(const std::string& text, bool masked = true) {
  auto frame = FrameBuilder::CreateTextFrame(text);
  if (masked) {
    // 添加掩码
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    frame[1] |= 0x80;  // 设置掩码位
    std::vector<uint8_t> masked_frame(frame.begin(), frame.begin() + 2);
    masked_frame.insert(masked_frame.end(), mask, mask + 4);
    for (size_t i = 0; i < text.size(); ++i) {
      masked_frame.push_back(static_cast<uint8_t>(text[i]) ^ mask[i % 4]);
    }
    return masked_frame;
  }
  return frame;
}

}  // namespace

TEST(FrameParser, ParseMinimalTextFrame) {
  auto frame_data = CreateTextFrame("Hello", true);

  FrameParser parser;
  size_t consumed = 0;
  auto frame = parser.Parse(frame_data, consumed);

  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->opcode, OpCode::kText);
  EXPECT_EQ(frame->payload_length, 5);

  std::string payload(frame->payload.begin(), frame->payload.end());
  EXPECT_EQ(payload, "Hello");
  EXPECT_EQ(consumed, frame_data.size());
}

TEST(FrameParser, IsComplete) {
  auto frame_data = CreateTextFrame("Hello", true);

  FrameParser parser;
  EXPECT_TRUE(parser.IsComplete(frame_data));

  // 不完整帧
  std::vector<uint8_t> partial(frame_data.begin(), frame_data.begin() + 5);
  EXPECT_FALSE(parser.IsComplete(partial));
}

TEST(FrameParser, UnmaskedFrame) {
  auto frame_data = FrameBuilder::CreateTextFrame("World");

  FrameParser parser;
  size_t consumed = 0;
  auto frame = parser.Parse(frame_data, consumed);

  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->opcode, OpCode::kText);
  EXPECT_FALSE(frame->masked);

  std::string payload(frame->payload.begin(), frame->payload.end());
  EXPECT_EQ(payload, "World");
}

TEST(FrameParser, PingPongFrame) {
  auto ping_data = FrameBuilder::CreatePingFrame();

  FrameParser parser;
  size_t consumed = 0;
  auto ping = parser.Parse(ping_data, consumed);

  ASSERT_TRUE(ping.has_value());
  EXPECT_EQ(ping->opcode, OpCode::kPing);

  auto pong_data = FrameBuilder::CreatePongFrame(ping->payload);
  auto pong = parser.Parse(pong_data, consumed);

  ASSERT_TRUE(pong.has_value());
  EXPECT_EQ(pong->opcode, OpCode::kPong);
}

TEST(FrameParser, CloseFrame) {
  auto close_data = FrameBuilder::CreateCloseFrame(1000, "Normal close");

  FrameParser parser;
  size_t consumed = 0;
  auto close = parser.Parse(close_data, consumed);

  ASSERT_TRUE(close.has_value());
  EXPECT_EQ(close->opcode, OpCode::kClose);
  EXPECT_GE(close->payload.size(), 2);
}

TEST(FrameParser, BinaryFrame) {
  std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0x03};
  auto frame_data = FrameBuilder::CreateBinaryFrame(binary_data);

  FrameParser parser;
  size_t consumed = 0;
  auto frame = parser.Parse(frame_data, consumed);

  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->opcode, OpCode::kBinary);
  EXPECT_EQ(frame->payload, binary_data);
}

TEST(FrameParser, EmptyFrame) {
  auto frame_data = FrameBuilder::CreateTextFrame("");

  FrameParser parser;
  size_t consumed = 0;
  auto frame = parser.Parse(frame_data, consumed);

  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->payload.size(), 0);
}

TEST(FrameParser, IncompleteFrame) {
  std::vector<uint8_t> incomplete = {0x81};  // FIN + Text opcode, no length

  FrameParser parser;
  size_t consumed = 0;
  auto frame = parser.Parse(incomplete, consumed);

  EXPECT_FALSE(frame.has_value());
}
