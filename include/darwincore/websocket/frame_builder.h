// Copyright (C) 2025
// Licensed under the MIT License

#pragma once

#include <cstdint>
#include <vector>

#include "darwincore/websocket/frame_parser.h"

namespace darwincore {
namespace websocket {
/**
 * @brief WebSocket 帧构建器
 * 
 * 负责将数据编码为 WebSocket 帧。
 * 支持所有标准帧类型（文本、二进制、控制帧）。
 * 
 * 特性：
 * - 服务器发送的帧不掩码
 * - 自动设置 FIN 位
 */
class FrameBuilder {
public:
    /**
     * @brief 创建文本帧
     * @param text 文本内容
     * @return 编码后的帧数据
     */
    static std::vector<uint8_t> CreateTextFrame(const std::string& text);
    
    /**
     * @brief 创建二进制帧
     * @param data 二进制数据
     * @return 编码后的帧数据
     */
    static std::vector<uint8_t> CreateBinaryFrame(const std::vector<uint8_t>& data);
    
    /**
     * @brief 创建 Ping 帧
     * @param data Ping 数据
     * @return 编码后的帧数据
     */
    static std::vector<uint8_t> CreatePingFrame(const std::vector<uint8_t>& data);
    
    /**
     * @brief 创建 Pong 帧
     * @param data Pong 数据（通常与 Ping 数据相同）
     * @return 编码后的帧数据
     */
    static std::vector<uint8_t> CreatePongFrame(const std::vector<uint8_t>& data);
    
    /**
     * @brief 创建 Close 帧
     * @param code 关闭码（默认 1000 正常关闭）
     * @param reason 关闭原因
     * @return 编码后的帧数据
     */
    static std::vector<uint8_t> CreateCloseFrame(uint16_t code = 1000,
                                                  const std::string& reason = "");

    /**
     * @brief 通用帧构建函数（公开供服务器内部使用）
     * @param opcode 操作码
     * @param payload 载荷数据
     * @return 编码后的帧数据
     */
    static std::vector<uint8_t> BuildFrame(OpCode opcode,
                                            const std::vector<uint8_t>& payload);

private:
    /**
     * @brief 计算帧头大小
     */
    static size_t CalculateHeaderSize(uint64_t payload_length);
    
    /**
     * @brief 写入可变长度整数
     */
    static void WriteVarInt(std::vector<uint8_t>& buffer,
                            uint64_t value,
                            size_t offset);
};

}  // namespace websocket
}  // namespace darwincore
