// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/frame_builder.h"
#include "darwincore/websocket/frame_parser.h"

using namespace darwincore::websocket;

class FrameBuilderTest : public ::testing::Test {
protected:
    FrameParser parser_;
};

TEST_F(FrameBuilderTest, CreateTextFrame) {
    auto frame = FrameBuilder::CreateTextFrame("Hello");
    
    EXPECT_EQ(frame[0], 0x81);  // FIN + TEXT
    EXPECT_EQ(frame[1], 5);     // Length
    EXPECT_EQ(frame[2], 'H');
    EXPECT_EQ(frame[3], 'e');
    EXPECT_EQ(frame[4], 'l');
    EXPECT_EQ(frame[5], 'l');
    EXPECT_EQ(frame[6], 'o');
}

TEST_F(FrameBuilderTest, CreateBinaryFrame) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto frame = FrameBuilder::CreateBinaryFrame(data);
    
    EXPECT_EQ(frame[0], 0x82);  // FIN + BINARY
    EXPECT_EQ(frame[1], 3);     // Length
}

TEST_F(FrameBuilderTest, CreatePingFrame) {
    std::vector<uint8_t> ping_data = {0x01, 0x02, 0x03};
    auto frame = FrameBuilder::CreatePingFrame(ping_data);
    
    EXPECT_EQ(frame[0], 0x89);  // FIN + PING
    EXPECT_EQ(frame[1], 3);     // Length
}

TEST_F(FrameBuilderTest, CreatePongFrame) {
    std::vector<uint8_t> pong_data = {0x01, 0x02, 0x03};
    auto frame = FrameBuilder::CreatePongFrame(pong_data);
    
    EXPECT_EQ(frame[0], 0x8A);  // FIN + PONG
    EXPECT_EQ(frame[1], 3);     // Length
}

TEST_F(FrameBuilderTest, CreateCloseFrame) {
    auto frame = FrameBuilder::CreateCloseFrame(1000, "Connection closed");

    EXPECT_EQ(frame[0], 0x88);  // FIN + CLOSE
    EXPECT_EQ(frame[1], 19);    // Length: 2 (code) + 17 (string "Connection closed")
    EXPECT_EQ(frame[2], 0x03);  // High byte of 1000
    EXPECT_EQ(frame[3], 0xE8);  // Low byte of 1000
}

TEST_F(FrameBuilderTest, RoundTripTextFrame) {
    std::string original = "Hello, WebSocket!";
    auto frame = FrameBuilder::CreateTextFrame(original);
    
    size_t consumed = 0;
    auto parsed = parser_.Parse(frame, consumed);
    
    ASSERT_TRUE(parsed.has_value());
    std::string result(parsed->payload.begin(), parsed->payload.end());
    EXPECT_EQ(result, original);
}

TEST_F(FrameBuilderTest, RoundTripBinaryFrame) {
    std::vector<uint8_t> original = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto frame = FrameBuilder::CreateBinaryFrame(original);
    
    size_t consumed = 0;
    auto parsed = parser_.Parse(frame, consumed);
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->payload, original);
}

TEST_F(FrameBuilderTest, LargePayload) {
    // 创建 200 字节的载荷
    std::string original(200, 'X');
    auto frame = FrameBuilder::CreateTextFrame(original);
    
    size_t consumed = 0;
    auto parsed = parser_.Parse(frame, consumed);
    
    ASSERT_TRUE(parsed.has_value());
    std::string result(parsed->payload.begin(), parsed->payload.end());
    EXPECT_EQ(result, original);
}
