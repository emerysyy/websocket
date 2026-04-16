// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/frame_parser.h"

#include <stdexcept>

namespace darwincore {
namespace websocket {

std::optional<Frame> FrameParser::Parse(const std::vector<uint8_t>& data,
                                        size_t& consumed) {
  consumed = 0;

  if (data.empty()) {
    return std::nullopt;
  }

  // 解析帧头
  bool fin = false;
  OpCode opcode = OpCode::kContinuation;
  bool masked = false;
  uint64_t payload_length = 0;
  uint8_t masking_key[4] = {0};

  size_t header_size = ParseHeader(data, fin, opcode, masked,
                                  payload_length, masking_key);
  if (header_size == 0) {
    return std::nullopt;
  }

  // 验证载荷长度
  constexpr uint64_t kMaxPayloadLength = 0x7FFFFFFFFFFFFFFFULL;
  if (payload_length > kMaxPayloadLength) {
    throw std::runtime_error("Payload length exceeds maximum");
  }

  // 检查数据完整性
  size_t total_frame_size = header_size + static_cast<size_t>(payload_length);
  if (data.size() < total_frame_size) {
    return std::nullopt;
  }

  // 提取载荷数据
  std::vector<uint8_t> payload;
  if (payload_length > 0) {
    payload.assign(data.begin() + header_size,
                   data.begin() + header_size + payload_length);

    // 解除掩码
    if (masked) {
      Unmask(payload, masking_key);
    }
  }

  consumed = total_frame_size;

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
  if (data.empty()) return false;

  size_t min_size = CalculateMinFrameSize(data);
  if (data.size() < min_size) return false;

  bool masked = (data[1] & 0x80) != 0;
  uint64_t payload_len = data[1] & 0x7F;
  size_t header_size = 2;

  if (payload_len == 126) {
    header_size = 4;
  } else if (payload_len == 127) {
    header_size = 10;
  }

  if (masked) {
    header_size += 4;
  }

  if (payload_len <= 125) {
    // 直接使用
  } else if (payload_len == 126) {
    if (data.size() < 4) return false;
    payload_len = (static_cast<uint64_t>(data[2]) << 8) |
                  static_cast<uint64_t>(data[3]);
  } else if (payload_len == 127) {
    if (data.size() < 10) return false;
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | static_cast<uint64_t>(data[2 + i]);
    }
  }

  size_t total_frame_size = header_size + static_cast<size_t>(payload_len);
  return data.size() >= total_frame_size;
}

size_t FrameParser::ParseHeader(const std::vector<uint8_t>& data,
                                bool& out_fin, OpCode& out_opcode,
                                bool& out_masked,
                                uint64_t& out_payload_length,
                                uint8_t* out_masking_key) {
  if (data.size() < 2) return 0;

  uint8_t byte0 = data[0];
  uint8_t byte1 = data[1];

  out_fin = (byte0 & 0x80) != 0;
  out_opcode = static_cast<OpCode>(byte0 & 0x0F);
  out_masked = (byte1 & 0x80) != 0;
  uint64_t payload_len = byte1 & 0x7F;

  size_t header_size = 2;

  if (payload_len <= 125) {
    out_payload_length = payload_len;
  } else if (payload_len == 126) {
    if (data.size() < 4) return 0;
    out_payload_length = (static_cast<uint64_t>(data[2]) << 8) |
                         static_cast<uint64_t>(data[3]);
    header_size = 4;
  } else if (payload_len == 127) {
    if (data.size() < 10) return 0;
    out_payload_length = 0;
    for (int i = 0; i < 8; ++i) {
      out_payload_length = (out_payload_length << 8) |
                           static_cast<uint64_t>(data[2 + i]);
    }
    header_size = 10;
  }

  if (out_masked) {
    size_t masking_offset = header_size;
    if (data.size() < masking_offset + 4) return 0;
    if (out_masking_key != nullptr) {
      std::copy_n(data.begin() + masking_offset, 4, out_masking_key);
    }
    header_size += 4;
  }

  return header_size;
}

size_t FrameParser::CalculateMinFrameSize(const std::vector<uint8_t>& data) const {
  if (data.size() < 2) return 2;

  bool masked = (data[1] & 0x80) != 0;
  uint64_t payload_len = data[1] & 0x7F;
  size_t min_size = 2;

  if (payload_len == 126) {
    min_size += 2;
  } else if (payload_len == 127) {
    min_size += 8;
  }

  if (masked) {
    min_size += 4;
  }

  return min_size;
}

void FrameParser::Unmask(std::vector<uint8_t>& payload,
                         const uint8_t* masking_key) const {
  for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] ^= masking_key[i % 4];
  }
}

}  // namespace websocket
}  // namespace darwincore
