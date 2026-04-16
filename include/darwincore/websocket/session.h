// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_WEBSOCKET_SESSION_H_
#define DARWINCORE_WEBSOCKET_SESSION_H_

#include <memory>

#include "frame_parser.h"

namespace darwincore {
namespace websocket {

/**
 * @brief WebSocket 会话阶段
 */
enum class SessionPhase {
  kHandshake,   // 等待握手
  kWebSocket,   // WebSocket 通信中
  kClosing,     // 正在关闭
  kClosed,      // 已关闭
};

/**
 * @brief WebSocket 会话状态
 *
 * 把单连接的协议状态挂到 Connection::context_ 上。
 * 一个连接对应一个 session，不维护全局连接表。
 */
class WebSocketSession {
 public:
  WebSocketSession();
  ~WebSocketSession() = default;

  /**
   * @brief 获取当前阶段
   */
  SessionPhase phase() const { return phase_; }

  /**
   * @brief 设置当前阶段
   */
  void set_phase(SessionPhase phase) { phase_ = phase; }

  /**
   * @brief 获取帧解析器
   */
  FrameParser* parser() { return &parser_; }

  /**
   * @brief 检查是否正在关闭
   */
  bool is_closing() const { return is_closing_; }

  /**
   * @brief 标记为正在关闭
   */
  void set_closing(bool closing) { is_closing_ = closing; }

  /**
   * @brief 获取接收缓冲区起始偏移量
   */
  size_t processed_offset() const { return processed_offset_; }

  /**
   * @brief 设置接收缓冲区偏移量
   */
  void set_processed_offset(size_t offset) { processed_offset_ = offset; }

  /**
   * @brief 重置会话状态
   */
  void Reset();

 private:
  SessionPhase phase_ = SessionPhase::kHandshake;
  FrameParser parser_;
  bool is_closing_ = false;
  size_t processed_offset_ = 0;
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_WEBSOCKET_SESSION_H_
