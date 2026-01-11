// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/frame_parser.h"

using namespace darwincore::websocket;

class FrameParserTest : public ::testing::Test {
protected:
    FrameParser parser_;
};

TEST_F(FrameParserTest, ParseSimpleTextFrame) {
    // 未掩码的文本帧 "Hello"
    std::vector<uint8_t> data = {
        0x81, 0x05,  // FIN=1, opcode=TEXT, len=5
        'H', 'e', 'l', 'l', 'o'
    };
    size_t consumed = 0;
    
    auto frame = parser_.Parse(data, consumed);
    
    ASSERT_TRUE(frame.has_value());
    EXPECT_TRUE(frame->fin);
    EXPECT_EQ(frame->opcode, OpCode::kText);
    EXPECT_FALSE(frame->masked);
    EXPECT_EQ(frame->payload_length, 5);
    EXPECT_EQ(consumed, 7);
    
    std::string text(frame->payload.begin(), frame->payload.end());
    EXPECT_EQ(text, "Hello");
}

TEST_F(FrameParserTest, ParseMaskedTextFrame) {
    // 客户端发送的掩码文本帧
    std::vector<uint8_t> data = {
        0x81, 0x85,              // FIN=1, opcode=TEXT, MASK=1, len=5
        0x37, 0xfa, 0x21, 0x3d,  // Masking key
        0x7f, 0x9f, 0x4d, 0x51, 0x58  // Masked "Hello"
    };
    size_t consumed = 0;
    
    auto frame = parser_.Parse(data, consumed);
    
    ASSERT_TRUE(frame.has_value());
    EXPECT_TRUE(frame->masked);
    EXPECT_TRUE(frame->fin);
    EXPECT_EQ(frame->opcode, OpCode::kText);
    
    std::string text(frame->payload.begin(), frame->payload.end());
    EXPECT_EQ(text, "Hello");
}

TEST_F(FrameParserTest, ParseExtendedLength126) {
    // 长度 126 使用 2 字节扩展长度
    std::vector<uint8_t> data = {
        0x81, 0x7E,        // FIN=1, opcode=TEXT, len=126
        0x00, 0x7E         // Extended length = 126
    };
    // 添加 126 个 'A'（未掩码）
    std::vector<uint8_t> payload(126, 'A');
    data.insert(data.end(), payload.begin(), payload.end());
    
    size_t consumed = 0;
    
    auto frame = parser_.Parse(data, consumed);
    
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->payload_length, 126);
    EXPECT_EQ(frame->payload.size(), 126);
}

TEST_F(FrameParserTest, ParseIncompleteFrame) {
    // 不完整的帧（只有头部）
    std::vector<uint8_t> data = {0x81, 0x85, 0x12, 0x34};
    size_t consumed = 0;
    
    auto frame = parser_.Parse(data, consumed);
    
    EXPECT_FALSE(frame.has_value());
    EXPECT_EQ(consumed, 0);
}

TEST_F(FrameParserTest, ParsePingFrame) {
    // Ping 帧
    std::vector<uint8_t> data = {
        0x89, 0x00  // FIN=1, opcode=PING, len=0
    };
    size_t consumed = 0;
    
    auto frame = parser_.Parse(data, consumed);
    
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->opcode, OpCode::kPing);
    EXPECT_EQ(frame->payload_length, 0);
}

TEST_F(FrameParserTest, ParseCloseFrame) {
    // Close 帧带状态码
    std::vector<uint8_t> data = {
        0x88, 0x02,  // FIN=1, opcode=CLOSE, len=2
        0x03, 0xE8   // Status code 1000
    };
    size_t consumed = 0;
    
    auto frame = parser_.Parse(data, consumed);
    
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->opcode, OpCode::kClose);
    EXPECT_EQ(frame->payload_length, 2);
    EXPECT_EQ(frame->payload[0], 0x03);
    EXPECT_EQ(frame->payload[1], 0xE8);
}

TEST_F(FrameParserTest, IsComplete) {
    // 不完整数据
    std::vector<uint8_t> incomplete = {0x81, 0x05};
    EXPECT_FALSE(parser_.IsComplete(incomplete));
    
    // 完整数据
    std::vector<uint8_t> complete = {
        0x81, 0x05, 'H', 'e', 'l', 'l', 'o'
    };
    EXPECT_TRUE(parser_.IsComplete(complete));
}
