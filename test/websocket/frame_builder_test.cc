// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/frame_builder.h"

namespace {

using namespace darwincore::websocket;

bool IsValidFrame(const std::vector<uint8_t>& frame) {
  if (frame.size() < 2) return false;
  // 第一个字节应该是 0x81 (FIN + Text) 或 0x82 (FIN + Binary)
  return (frame[0] & 0x0F) <= 0x0A;
}

}  // namespace

TEST(FrameBuilder, CreateTextFrame) {
  auto frame = FrameBuilder::CreateTextFrame("Hello");

  ASSERT_GE(frame.size(), 2);
  EXPECT_EQ(frame[0], 0x81);  // FIN + Text
  EXPECT_EQ(frame[1], 5);       // Length = 5
  EXPECT_EQ(frame[2], 'H');
  EXPECT_EQ(frame[3], 'e');
  EXPECT_EQ(frame[4], 'l');
  EXPECT_EQ(frame[5], 'l');
  EXPECT_EQ(frame[6], 'o');
}

TEST(FrameBuilder, CreateBinaryFrame) {
  std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
  auto frame = FrameBuilder::CreateBinaryFrame(data);

  ASSERT_GE(frame.size(), 2);
  EXPECT_EQ(frame[0], 0x82);  // FIN + Binary
  EXPECT_EQ(frame[1], 4);       // Length = 4
}

TEST(FrameBuilder, CreatePingFrame) {
  std::vector<uint8_t> payload = {'p', 'i', 'n', 'g'};
  auto frame = FrameBuilder::CreatePingFrame(payload);

  ASSERT_GE(frame.size(), 2);
  EXPECT_EQ(frame[0], 0x89);  // FIN + Ping
  EXPECT_EQ(frame[1], 4);
}

TEST(FrameBuilder, CreatePongFrame) {
  std::vector<uint8_t> payload = {'p', 'o', 'n', 'g'};
  auto frame = FrameBuilder::CreatePongFrame(payload);

  ASSERT_GE(frame.size(), 2);
  EXPECT_EQ(frame[0], 0x8A);  // FIN + Pong
  EXPECT_EQ(frame[1], 4);
}

TEST(FrameBuilder, CreateCloseFrame) {
  auto frame = FrameBuilder::CreateCloseFrame(1000, "Bye");

  ASSERT_GE(frame.size(), 4);
  EXPECT_EQ(frame[0], 0x88);  // FIN + Close
  EXPECT_EQ(frame[2], 0x03);  // Close code high
  EXPECT_EQ(frame[3], 0xE8);  // Close code low (1000 = 0x03E8)
  EXPECT_EQ(frame[4], 'B');
  EXPECT_EQ(frame[5], 'y');
  EXPECT_EQ(frame[6], 'e');
}

TEST(FrameBuilder, CreateCloseFrameWithoutReason) {
  auto frame = FrameBuilder::CreateCloseFrame();

  ASSERT_GE(frame.size(), 4);
  EXPECT_EQ(frame[0], 0x88);  // FIN + Close
  EXPECT_EQ(frame[1], 2);     // Length = 2 (just the code)
}

TEST(FrameBuilder, BuildFrameWithLength126) {
  std::string large_payload(200, 'x');
  auto frame = FrameBuilder::CreateTextFrame(large_payload);

  ASSERT_GE(frame.size(), 4);
  EXPECT_EQ(frame[0], 0x81);
  EXPECT_EQ(frame[1], 126);  // Extended length indicator
  EXPECT_EQ(frame[2], 0x00);  // High byte of 200
  EXPECT_EQ(frame[3], 0xC8);  // Low byte of 200 (200 = 0xC8)
}

TEST(FrameBuilder, BuildFrameWithLength127) {
  std::string large_payload(70000, 'x');
  auto frame = FrameBuilder::CreateTextFrame(large_payload);

  ASSERT_GE(frame.size(), 11);
  EXPECT_EQ(frame[0], 0x81);
  EXPECT_EQ(frame[1], 127);  // Extended length indicator
  // 70000 = 0x11158, big-endian 8 bytes: 00 00 00 00 00 00 01 11 58
  EXPECT_EQ(frame[2], 0x00);  // MSB (70000 >> 56)
  EXPECT_EQ(frame[3], 0x00);  // (70000 >> 48)
  EXPECT_EQ(frame[4], 0x00);  // (70000 >> 40)
  EXPECT_EQ(frame[5], 0x00);  // (70000 >> 32)
  EXPECT_EQ(frame[6], 0x00);  // (70000 >> 24)
  EXPECT_EQ(frame[7], 0x01);  // (70000 >> 16) = 1
  EXPECT_EQ(frame[8], 0x11);  // (70000 >> 8) & 0xFF = 0x11
  EXPECT_EQ(frame[9], 0x70);  // 70000 & 0xFF = 0x70
}

TEST(FrameBuilder, EmptyPayload) {
  auto frame = FrameBuilder::CreateTextFrame("");

  ASSERT_GE(frame.size(), 2);
  EXPECT_EQ(frame[0], 0x81);
  EXPECT_EQ(frame[1], 0);  // Empty payload
}

TEST(FrameBuilder, CloseFrameTruncatesLongReason) {
  // RFC6455: 控制帧载荷必须 <= 125 字节
  // Close 帧: 2 字节 code + reason <= 125, 所以 reason <= 123
  std::string long_reason(200, 'x');
  auto frame = FrameBuilder::CreateCloseFrame(1000, long_reason);

  ASSERT_GE(frame.size(), 4);
  EXPECT_EQ(frame[0], 0x88);  // FIN + Close
  // 载荷 = 2 (code) + 123 (truncated reason) = 125
  EXPECT_EQ(frame[1], 125);
}

TEST(FrameBuilder, PingFrameTruncatesLongPayload) {
  // RFC6455: 控制帧载荷必须 <= 125 字节
  std::vector<uint8_t> long_payload(200, 'x');
  auto frame = FrameBuilder::CreatePingFrame(long_payload);

  ASSERT_GE(frame.size(), 2);
  EXPECT_EQ(frame[0], 0x89);  // FIN + Ping
  EXPECT_EQ(frame[1], 125);  // Truncated to 125
}

TEST(FrameBuilder, PongFrameTruncatesLongPayload) {
  // RFC6455: 控制帧载荷必须 <= 125 字节
  std::vector<uint8_t> long_payload(200, 'x');
  auto frame = FrameBuilder::CreatePongFrame(long_payload);

  ASSERT_GE(frame.size(), 2);
  EXPECT_EQ(frame[0], 0x8A);  // FIN + Pong
  EXPECT_EQ(frame[1], 125);  // Truncated to 125
}
