// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_WEBSOCKET_FRAME_PARSER_H_
#define DARWINCORE_WEBSOCKET_FRAME_PARSER_H_

#include <cstdint>
#include <optional>
#include <string>
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
  // 0x3-0x7: 保留给非控制帧
  kClose = 0x8,
  kPing = 0x9,
  kPong = 0xA,
  // 0xB-0xF: 保留给非控制帧
};

/**
 * @brief WebSocket 帧解析错误类型
 */
enum class ParseError {
  kNone = 0,
  kFragmentedControlFrame,    // 控制帧不能被分片 (RFC 6455 5.4)
  kControlFrameTooLarge,      // 控制帧 payload 不能超过 125 字节 (RFC 6455 5.5)
  kInvalidOpCode,            // 无效的操作码 (RFC 6455 5.2)
  kReservedBitSet,           // RSV1/RSV2/RSV3 必须为 0 (RFC 6455 5.2)
  kIncompleteFrame,         // 数据不完整
  kMaskedServerFrame,       // 服务端到服务端帧不能掩码 (RFC 6455 5.1)
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
 * 解析时自动校验控制帧协议约束。
 */
class FrameParser {
 public:
  FrameParser() = default;
  ~FrameParser() = default;

  /**
   * @brief 解析 WebSocket 帧
   * @param data 输入数据（可能包含多个帧或部分帧）
   * @param consumed 输出参数，表示本次解析消耗的字节数
   * @return 如果数据完整且协议正确，返回解析的 Frame
   *         如果数据不完整，返回 nullopt（不算错误）
   *         如果协议违规，抛出 ParseError 异常
   */
  std::optional<Frame> Parse(const std::vector<uint8_t>& data, size_t& consumed);

  /**
   * @brief 检查数据是否包含一个完整的 WebSocket 帧
   * @param data 输入数据
   * @return 如果包含完整帧返回 true，否则返回 false
   */
  bool IsComplete(const std::vector<uint8_t>& data) const;

  /**
   * @brief 获取最近一次协议约束错误
   * @return 错误类型
   */
  ParseError GetLastConstraintError() const { return last_constraint_error_; }

  /**
   * @brief 校验 WebSocket 帧的控制帧协议约束
   * @param frame 要校验的帧
   * @throw 如果违反协议约束，抛出 ParseError
   *
   * RFC 6455 约束：
   * - 控制帧不能被分片 (FIN 必须为 1)
   * - 控制帧 payload 不能超过 125 字节
   */
  void ValidateControlFrameConstraints(const Frame& frame);

  /**
   * @brief 检查是否是控制帧 opcode
   */
  static bool IsControlFrame(OpCode opcode) {
    return opcode == OpCode::kClose ||
           opcode == OpCode::kPing ||
           opcode == OpCode::kPong;
  }

 private:
  /**
   * @brief 校验控制帧协议约束（内部版本）
   * @param fin FIN 标志
   * @param opcode 操作码
   * @param payload_length 载荷长度
   * @throw 如果违反协议约束，抛出 ParseError
   */
  void ValidateControlFrameConstraints(bool fin, OpCode opcode,
                                       uint64_t payload_length) const;

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

  // 最近一次协议约束错误
  mutable ParseError last_constraint_error_ = ParseError::kNone;
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_WEBSOCKET_FRAME_PARSER_H_
