// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_WEBSOCKET_FRAME_PARSER_H_
#define DARWINCORE_WEBSOCKET_FRAME_PARSER_H_

#include <cstdint>
#include <optional>
#include <vector>

namespace darwincore {
namespace websocket {

/**
 * @brief WebSocket 操作码 (RFC 6455)
 */
enum class OpCode : uint8_t {
  kContinuation = 0x0,
  kText = 0x1,
  kBinary = 0x2,
  kClose = 0x8,
  kPing = 0x9,
  kPong = 0xA,
};

/**
 * @brief WebSocket 帧结构
 */
struct Frame {
  bool fin = true;                  // FIN 标志
  OpCode opcode = OpCode::kText;     // 操作码
  bool masked = false;               // 是否掩码
  uint64_t payload_length = 0;       // 载荷长度
  uint8_t masking_key[4] = {0};      // 掩码密钥
  std::vector<uint8_t> payload;      // 载荷数据
};

/**
 * @brief WebSocket 帧解析器
 *
 * 将原始字节流解析成 WebSocket 帧。
 * 支持 RFC 6455 定义的所有帧类型。
 */
class FrameParser {
 public:
  FrameParser() = default;
  ~FrameParser() = default;

  /**
   * @brief 解析 WebSocket 帧
   * @param data 输入数据（可能包含多个帧或部分帧）
   * @param consumed 输出参数，表示本次解析消耗的字节数
   * @return 如果数据完整，返回解析的 Frame；否则返回 nullopt
   */
  std::optional<Frame> Parse(const std::vector<uint8_t>& data, size_t& consumed);

  /**
   * @brief 检查数据是否包含一个完整的 WebSocket 帧
   * @param data 输入数据
   * @return 如果包含完整帧返回 true，否则返回 false
   */
  bool IsComplete(const std::vector<uint8_t>& data) const;

 private:
  /**
   * @brief 解析帧头
   * @param data 输入数据
   * @param out_fin 输出：FIN 标志
   * @param out_opcode 输出：操作码
   * @param out_masked 输出：掩码标志
   * @param out_payload_length 输出：载荷长度
   * @param out_masking_key 输出：掩码密钥（4 字节）
   * @return 帧头大小（字节），如果数据不完整返回 0
   */
  size_t ParseHeader(const std::vector<uint8_t>& data, bool& out_fin,
                     OpCode& out_opcode, bool& out_masked,
                     uint64_t& out_payload_length, uint8_t* out_masking_key);

  /**
   * @brief 计算完整帧的最小大小（帧头部分）
   * @param data 输入数据
   * @return 帧头大小（字节）
   */
  size_t CalculateMinFrameSize(const std::vector<uint8_t>& data) const;

  /**
   * @brief 解除载荷掩码（原地修改）
   * @param payload 载荷数据
   * @param masking_key 4 字节掩码密钥
   */
  void Unmask(std::vector<uint8_t>& payload, const uint8_t* masking_key) const;
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_WEBSOCKET_FRAME_PARSER_H_
