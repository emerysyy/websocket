// Copyright (C) 2025
// Licensed under the MIT License

/**
 * @file frame_builder.cc
 * @brief WebSocket 帧构建器实现
 *
 * 根据 RFC 6455 标准实现 WebSocket 数据帧的构建功能
 * 负责将应用层数据封装成 WebSocket 协议格式的帧
 */

#include "darwincore/websocket/frame_builder.h"
#include "darwincore/websocket/frame_parser.h"

#include <algorithm>

namespace darwincore {
namespace websocket {

std::vector<uint8_t> FrameBuilder::CreateTextFrame(const std::string& text) {
    // 将字符串转换为字节数组作为载荷
    std::vector<uint8_t> payload(text.begin(), text.end());
    return BuildFrame(OpCode::kText, payload);
}

std::vector<uint8_t> FrameBuilder::CreateBinaryFrame(const std::vector<uint8_t>& data) {
    // 直接使用二进制数据作为载荷
    return BuildFrame(OpCode::kBinary, data);
}

std::vector<uint8_t> FrameBuilder::CreatePingFrame(const std::vector<uint8_t>& data) {
    // Ping 帧用于心跳检测
    return BuildFrame(OpCode::kPing, data);
}

std::vector<uint8_t> FrameBuilder::CreatePongFrame(const std::vector<uint8_t>& data) {
    // Pong 帧是对 Ping 的响应
    return BuildFrame(OpCode::kPong, data);
}

std::vector<uint8_t> FrameBuilder::CreateCloseFrame(uint16_t code,
                                                     const std::string& reason) {
    /**
     * 关闭帧格式：
     * - 2 字节：关闭状态码（大端序）
     * - N 字节：关闭原因（UTF-8 编码的字符串）
     *
     * 常见状态码：
     * - 1000: 正常关闭
     * - 1001: 端点离开
     * - 1002: 协议错误
     * - 1003: 不支持的数据类型
     */
    std::vector<uint8_t> payload;
    payload.reserve(2 + reason.size());

    // 将状态码按大端序写入（先高字节，后低字节）
    payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));  // 高字节
    payload.push_back(static_cast<uint8_t>(code & 0xFF));          // 低字节

    // 添加原因字符串
    for (char c : reason) {
        payload.push_back(static_cast<uint8_t>(c));
    }

    return BuildFrame(OpCode::kClose, payload);
}

std::vector<uint8_t> FrameBuilder::BuildFrame(OpCode opcode,
                                               const std::vector<uint8_t>& payload) {
    /**
     * WebSocket 帧结构（RFC 6455）：
     *
     *  0                   1                   2                   3
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-------+-+-------------+-------------------------------+
     * |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     * |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     * |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     * | |1|2|3|       |K|             |                               |
     * +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     * |     Extended payload length continued, if payload len == 127  |
     * + - - - - - - - - - - - - - - - +-------------------------------+
     * |                               |Masking-key, if MASK set to 1  |
     * +-------------------------------+-------------------------------+
     * | Masking-key (continued)       |          Payload Data         |
     * +-------------------------------- - - - - - - - - - - - - - - - +
     * :                     Payload Data continued ...                :
     * + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     * |                     Payload Data continued ...                |
     * +---------------------------------------------------------------+
     */

    uint64_t payload_length = payload.size();
    size_t header_size = CalculateHeaderSize(payload_length);

    // 预分配帧内存（帧头 + 载荷）
    std::vector<uint8_t> frame;
    frame.reserve(header_size + payload_length);

    // === 第一个字节：FIN + RSV + opcode ===
    // FIN: 1 表示这是最后一帧
    // RSV1-3: 0 保留位，必须为 0
    // opcode: 4 位操作码
    uint8_t byte0 = 0x80 | static_cast<uint8_t>(opcode);  // 0x80 = FIN=1, RSV=0
    frame.push_back(byte0);

    // === 第二个字节：MASK + Payload len ===
    // MASK: 0 表示服务器发送，不使用掩码
    // Payload len: 7 位载荷长度
    if (payload_length <= 125) {
        // 短载荷：长度直接编码在第二个字节中
        frame.push_back(static_cast<uint8_t>(payload_length));
    } else if (payload_length <= 65535) {
        // 中等载荷：使用 2 字节扩展长度
        frame.push_back(126);  // 扩展长度标识
        frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));  // 高字节
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));          // 低字节
    } else {
        // 超大载荷：使用 8 字节扩展长度
        frame.push_back(127);  // 扩展长度标识

        // 大端序写入 8 字节长度（先写最高字节）
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (i * 8)) & 0xFF));
        }
    }

    // === 添加载荷数据 ===
    // 服务器发送的数据不使用掩码，所以不需要 Masking-key
    if (!payload.empty()) {
        frame.insert(frame.end(), payload.begin(), payload.end());
    }

    return frame;
}

size_t FrameBuilder::CalculateHeaderSize(uint64_t payload_length) {
    /**
     * 计算帧头大小
     *
     * 帧头组成：
     * - 2 字节：基础帧头（FIN + opcode + MASK + Payload len）
     * - 0/2/8 字节：扩展载荷长度
     * - 0/4 字节：掩码密钥（仅客户端发送时）
     *
     * 服务器发送不使用掩码，所以只有前两部分
     */
    size_t header_size = 2;  // 基础帧头（2 字节）

    if (payload_length <= 125) {
        // 短载荷：长度编码在第二个字节中，无需额外字节
    } else if (payload_length <= 65535) {
        // 中等载荷：需要 2 字节扩展长度
        header_size += 2;
    } else {
        // 超大载荷：需要 8 字节扩展长度
        header_size += 8;
    }

    // 服务器发送的帧不掩码，所以没有掩码密钥部分

    return header_size;
}

}  // namespace websocket
}  // namespace darwincore
