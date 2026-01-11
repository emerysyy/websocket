// Copyright (C) 2025
// Licensed under the MIT License

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace darwincore {
namespace websocket {

/**
 * @brief WebSocket 操作码
 */
enum class OpCode : uint8_t {
    kContinuation = 0x0,
    kText = 0x1,
    kBinary = 0x2,
    kClose = 0x8,
    kPing = 0x9,
    kPong = 0xA
};

/**
 * @brief WebSocket 帧结构
 */
struct Frame {
    bool fin;                      ///< 是否为最后一帧
    OpCode opcode;                 ///< 操作码
    bool masked;                   ///< 是否使用掩码
    uint64_t payload_length;       ///< 载荷长度
    uint8_t masking_key[4];        ///< 掩码密钥
    std::vector<uint8_t> payload;  ///< 载荷数据
};

/**
 * @brief WebSocket 帧解析器
 * 
 * 负责将原始字节流解析为结构化的 WebSocket 帧。
 * 支持所有标准帧类型（文本、二进制、控制帧）。
 * 
 * 线程安全性：非线程安全，每个连接应该有独立的解析器实例。
 */
class FrameParser {
public:
    /**
     * @brief 解析 WebSocket 帧
     * 
     * 从输入数据中尝试解析一个完整的 WebSocket 帧。
     * 如果数据不完整，返回 std::nullopt 且 consumed 为 0。
     * 
     * @param data 输入数据缓冲区
     * @param consumed [out] 已消费的字节数（解析失败时为 0）
     * @return 成功时返回 Frame，数据不完整时返回 nullopt
     */
    std::optional<Frame> Parse(const std::vector<uint8_t>& data,
                               size_t& consumed);
    
    /**
     * @brief 检查数据是否包含完整帧
     */
    bool IsComplete(const std::vector<uint8_t>& data) const;

private:
    /**
     * @brief 解析帧头
     * @return 帧头字节数，0 表示数据不完整
     */
    size_t ParseHeader(const std::vector<uint8_t>& data,
                       bool& out_fin,
                       OpCode& out_opcode,
                       bool& out_masked,
                       uint64_t& out_payload_length,
                       uint8_t* out_masking_key);
    
    /**
     * @brief 计算完整帧所需的最小字节数
     */
    static size_t CalculateMinFrameSize(const std::vector<uint8_t>& data);
    
    /**
     * @brief 解除掩码
     * @param payload 被掩码的数据
     * @param masking_key 4 字节掩码密钥
     */
    void Unmask(std::vector<uint8_t>& payload,
                const uint8_t* masking_key) const;
};

}  // namespace websocket
}  // namespace darwincore
