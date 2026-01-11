// Copyright (C) 2025
// Licensed under the MIT License

/**
 * @file frame_parser.cc
 * @brief WebSocket 帧解析器实现
 *
 * 根据 RFC 6455 标准实现 WebSocket 数据帧的解析功能
 * 负责从原始 TCP 数据中提取 WebSocket 帧
 */

#include "darwincore/websocket/frame_parser.h"

#include <stdexcept>

namespace darwincore {
namespace websocket {

std::optional<Frame> FrameParser::Parse(const std::vector<uint8_t>& data,
                                         size_t& consumed) {
    /**
     * 解析 WebSocket 帧
     *
     * @param data 输入数据（可能包含多个帧或部分帧）
     * @param consumed 输出参数，表示本次解析消耗的字节数
     * @return 如果数据完整，返回解析的 Frame；否则返回 nullopt
     *
     * 解析流程：
     * 1. 解析帧头（FIN、opcode、MASK、payload length）
     * 2. 验证载荷长度是否合法
     * 3. 检查数据是否完整（帧头 + 载荷）
     * 4. 提取载荷数据
     * 5. 如果有掩码，解除掩码
     */
    consumed = 0;

    // 空数据直接返回
    if (data.empty()) {
        return std::nullopt;
    }

    // === 步骤 1：解析帧头 ===
    bool fin = false;           // 是否为最后一帧
    OpCode opcode = OpCode::kContinuation;  // 操作码
    bool masked = false;        // 是否使用掩码
    uint64_t payload_length = 0;  // 载荷长度
    uint8_t masking_key[4] = {0}; // 掩码密钥

    size_t header_size = ParseHeader(data, fin, opcode, masked,
                                     payload_length, masking_key);
    if (header_size == 0) {
        return std::nullopt;  // 数据不完整，无法解析帧头
    }

    // === 步骤 2：验证载荷长度 ===
    // WebSocket 规范限制最大载荷长度为 2^63 - 1
    constexpr uint64_t kMaxPayloadLength = 0x7FFFFFFFFFFFFFFFULL;
    if (payload_length > kMaxPayloadLength) {
        throw std::runtime_error("Payload length exceeds maximum");
    }

    // === 步骤 3：检查数据完整性 ===
    size_t total_frame_size = header_size + static_cast<size_t>(payload_length);
    if (data.size() < total_frame_size) {
        return std::nullopt;  // 数据不完整，缺少载荷部分
    }

    // === 步骤 4：提取载荷数据 ===
    std::vector<uint8_t> payload;
    if (payload_length > 0) {
        // 从数据中复制载荷部分
        payload.assign(data.begin() + header_size,
                       data.begin() + header_size + payload_length);

        // === 步骤 5：解除掩码（如果需要）===
        // 客户端发送的数据必须掩码，服务器发送的数据不掩码
        if (masked) {
            Unmask(payload, masking_key);
        }
    }

    // 记录消耗的字节数
    consumed = total_frame_size;

    // === 构建返回的 Frame 对象 ===
    Frame frame;
    frame.fin = fin;
    frame.opcode = opcode;
    frame.masked = masked;
    frame.payload_length = payload_length;
    if (masked) {
        std::copy_n(masking_key, 4, frame.masking_key);
    }
    frame.payload = std::move(payload);

    return frame;
}

bool FrameParser::IsComplete(const std::vector<uint8_t>& data) const {
    /**
     * 检查数据是否包含一个完整的 WebSocket 帧
     *
     * @param data 输入数据
     * @return 如果包含完整帧返回 true，否则返回 false
     *
     * 这个方法用于在不解析帧的情况下快速判断数据完整性
     */
    if (data.empty()) {
        return false;
    }

    // 计算完整帧的最小大小（至少需要帧头）
    size_t min_size = CalculateMinFrameSize(data);

    // CalculateMinFrameSize 只返回帧头大小
    // 我们还需要检查是否有足够的数据包含载荷
    if (data.size() < min_size) {
        return false;
    }

    // === 提取载荷长度 ===
    bool masked = (data[1] & 0x80) != 0;  // MASK 位
    uint64_t payload_len = data[1] & 0x7F; // 7 位载荷长度
    size_t header_size = 2;

    // 根据载荷长度标识计算实际帧头大小
    if (payload_len == 126) {
        // 2 字节扩展长度
        header_size = 4;
    } else if (payload_len == 127) {
        // 8 字节扩展长度
        header_size = 10;
    }

    // 如果有掩码，帧头包含 4 字节掩码密钥
    if (masked) {
        header_size += 4;
    }

    // === 读取实际载荷长度 ===
    if (payload_len <= 125) {
        // 载荷长度直接使用 payload_len
    } else if (payload_len == 126) {
        // 从扩展长度字段读取 2 字节载荷长度
        if (data.size() < 4) {
            return false;
        }
        payload_len = (static_cast<uint64_t>(data[2]) << 8) |
                      static_cast<uint64_t>(data[3]);
    } else if (payload_len == 127) {
        // 从扩展长度字段读取 8 字节载荷长度
        if (data.size() < 10) {
            return false;
        }
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) |
                         static_cast<uint64_t>(data[2 + i]);
        }
    }

    // 完整帧大小 = 帧头大小 + 载荷长度
    size_t total_frame_size = header_size + static_cast<size_t>(payload_len);
    return data.size() >= total_frame_size;
}

size_t FrameParser::ParseHeader(const std::vector<uint8_t>& data,
                                bool& out_fin,
                                OpCode& out_opcode,
                                bool& out_masked,
                                uint64_t& out_payload_length,
                                uint8_t* out_masking_key) {
    /**
     * 解析 WebSocket 帧头
     *
     * @param data 输入数据
     * @param out_fin 输出：FIN 标志
     * @param out_opcode 输出：操作码
     * @param out_masked 输出：掩码标志
     * @param out_payload_length 输出：载荷长度
     * @param out_masking_key 输出：掩码密钥（4 字节）
     * @return 帧头大小（字节），如果数据不完整返回 0
     *
     * 帧头结构：
     * - 2 字节：基础帧头（FIN + opcode + MASK + payload len）
     * - 0/2/8 字节：扩展载荷长度
     * - 0/4 字节：掩码密钥
     */
    // 最小帧头大小：2 字节
    if (data.size() < 2) {
        return 0;
    }

    // 读取前两个字节
    uint8_t byte0 = data[0];
    uint8_t byte1 = data[1];

    // === 第一个字节：FIN + RSV + opcode ===
    out_fin = (byte0 & 0x80) != 0;                    // 最高位：FIN
    out_opcode = static_cast<OpCode>(byte0 & 0x0F);   // 低 4 位：opcode

    // === 第二个字节：MASK + Payload len ===
    out_masked = (byte1 & 0x80) != 0;                 // 最高位：MASK
    uint64_t payload_len = byte1 & 0x7F;              // 低 7 位：payload len

    size_t header_size = 2;

    // === 解析扩展载荷长度 ===
    if (payload_len <= 125) {
        // 短载荷：长度直接编码在第二个字节中
        out_payload_length = payload_len;
    } else if (payload_len == 126) {
        // 中等载荷：需要读取 2 字节扩展长度
        if (data.size() < 4) {
            return 0;  // 数据不完整
        }
        // 大端序读取 2 字节长度
        out_payload_length = (static_cast<uint64_t>(data[2]) << 8) |
                             static_cast<uint64_t>(data[3]);
        header_size = 4;
    } else if (payload_len == 127) {
        // 超大载荷：需要读取 8 字节扩展长度
        if (data.size() < 10) {
            return 0;  // 数据不完整
        }
        // 大端序读取 8 字节长度
        out_payload_length = 0;
        for (int i = 0; i < 8; ++i) {
            out_payload_length = (out_payload_length << 8) |
                                 static_cast<uint64_t>(data[2 + i]);
        }
        header_size = 10;
    }

    // === 解析掩码密钥（如果存在）===
    if (out_masked) {
        size_t masking_offset = header_size;
        if (data.size() < masking_offset + 4) {
            return 0;  // 数据不完整，缺少掩码密钥
        }
        // 复制 4 字节掩码密钥
        if (out_masking_key != nullptr) {
            std::copy_n(data.begin() + masking_offset, 4, out_masking_key);
        }
        header_size += 4;
    }

    return header_size;
}

size_t FrameParser::CalculateMinFrameSize(const std::vector<uint8_t>& data) {
    /**
     * 计算完整帧的最小大小（帧头部分）
     *
     * @param data 输入数据
     * @return 帧头大小（字节）
     *
     * 这个方法只计算帧头大小，不包括载荷数据
     * 用于快速判断是否有足够的数据解析帧头
     */
    if (data.size() < 2) {
        return 2;  // 至少需要 2 字节基础帧头
    }

    // 解析第二个字节获取 MASK 和 payload len 标识
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    size_t min_size = 2;  // 基础帧头大小

    // 根据载荷长度标识计算扩展长度字段大小
    if (payload_len <= 125) {
        // 无扩展长度字段
    } else if (payload_len == 126) {
        min_size += 2;  // 2 字节扩展长度
    } else if (payload_len == 127) {
        min_size += 8;  // 8 字节扩展长度
    }

    // 如果有掩码，需要 4 字节掩码密钥
    if (masked) {
        min_size += 4;
    }

    return min_size;
}

void FrameParser::Unmask(std::vector<uint8_t>& payload,
                         const uint8_t* masking_key) const {
    /**
     * 解除掩码
     *
     * @param payload 载荷数据（就地修改）
     * @param masking_key 4 字节掩码密钥
     *
     * WebSocket 掩码算法（RFC 6455 第 5.3 节）：
     * 对于载荷中的第 i 个字节，使用掩码密钥的第 (i % 4) 个字节进行 XOR 操作
     *
     * 伪代码：
     *   j = 0
     *   for i in 0..payload.length-1:
     *     payload[i] = payload[i] ^ masking_key[j]
     *     j = (j + 1) % 4
     */
    for (size_t i = 0; i < payload.size(); ++i) {
        // 使用掩码密钥的第 (i % 4) 个字节进行 XOR
        payload[i] ^= masking_key[i % 4];
    }
}

}  // namespace websocket
}  // namespace darwincore
