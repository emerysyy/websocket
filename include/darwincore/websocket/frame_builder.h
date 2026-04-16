// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_WEBSOCKET_FRAME_BUILDER_H_
#define DARWINCORE_WEBSOCKET_FRAME_BUILDER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "frame_parser.h"

namespace darwincore {
namespace websocket {

/**
 * @brief WebSocket 帧构建器
 *
 * 将业务数据编码成 WebSocket 帧。
 * 服务器发送的帧不需要掩码。
 */
class FrameBuilder {
 public:
  FrameBuilder() = delete;
  ~FrameBuilder() = delete;

  /**
   * @brief 创建文本帧
   * @param message 文本消息
   * @return WebSocket 帧字节数据
   */
  static std::vector<uint8_t> CreateTextFrame(const std::string& message);

  /**
   * @brief 创建二进制帧
   * @param data 二进制数据
   * @return WebSocket 帧字节数据
   */
  static std::vector<uint8_t> CreateBinaryFrame(const std::vector<uint8_t>& data);

  /**
   * @brief 创建 Ping 帧
   * @param payload 可选的载荷数据
   * @return WebSocket 帧字节数据
   */
  static std::vector<uint8_t> CreatePingFrame(
      const std::vector<uint8_t>& payload = {});

  /**
   * @brief 创建 Pong 帧
   * @param payload 载荷数据（通常与 Ping 载荷相同）
   * @return WebSocket 帧字节数据
   */
  static std::vector<uint8_t> CreatePongFrame(
      const std::vector<uint8_t>& payload = {});

  /**
   * @brief 创建 Close 帧
   * @param code 关闭码（RFC 6455 定义）
   * @param reason 关闭原因
   * @return WebSocket 帧字节数据
   */
  static std::vector<uint8_t> CreateCloseFrame(uint16_t code = 1000,
                                                const std::string& reason = "");

  /**
   * @brief 构建通用 WebSocket 帧
   * @param opcode 操作码
   * @param payload 载荷数据
   * @param fin FIN 标志（默认为 true）
   * @return WebSocket 帧字节数据
   */
  static std::vector<uint8_t> BuildFrame(OpCode opcode,
                                         const std::vector<uint8_t>& payload,
                                         bool fin = true);

 private:
  /**
   * @brief 编码载荷长度到帧
   * @param frame 输出帧
   * @param length 载荷长度
   */
  static void EncodeLength(std::vector<uint8_t>& frame, uint64_t length);
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_WEBSOCKET_FRAME_BUILDER_H_
