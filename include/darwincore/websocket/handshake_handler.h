// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_WEBSOCKET_HANDSHAKE_HANDLER_H_
#define DARWINCORE_WEBSOCKET_HANDSHAKE_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

namespace darwincore {
namespace websocket {

/**
 * @brief WebSocket 握手错误类型
 */
enum class HandshakeError {
  kNone = 0,
  kMalformedRequest,
  kInvalidMethod,
  kInvalidHttpVersion,
  kMissingUpgrade,
  kMissingConnection,
  kMissingHost,
  kMissingKey,
  kInvalidKey,
  kMissingVersion,
  kInvalidVersion,
  kRequestTooLarge,
};

/**
 * @brief WebSocket 握手处理器
 *
 * 处理 HTTP Upgrade 到 WebSocket 的握手流程。
 * 验证客户端请求并生成正确的握手响应。
 */
class HandshakeHandler {
 public:
  HandshakeHandler() = default;
  ~HandshakeHandler() = default;

  /**
   * @brief 解析客户端握手请求
   * @param request 原始 HTTP 请求字符串
   * @return 解析成功返回 true
   */
  bool ParseRequest(const std::string& request);

  /**
   * @brief 生成握手成功响应
   * @return HTTP 101 Switching Protocols 响应字符串
   */
  std::string GenerateResponse() const;

  /**
   * @brief 生成握手错误响应
   * @param error 错误类型
   * @return HTTP 错误响应字符串
   */
  std::string GenerateErrorResponse(HandshakeError error);

  /**
   * @brief 检查握手是否有效
   * @return 有效返回 true
   */
  bool IsValid() const;

  /**
   * @brief 获取最近一次错误
   * @return 错误类型
   */
  HandshakeError GetLastError() const;

  /**
   * @brief 获取客户端发送的 Sec-WebSocket-Key
   * @return WebSocket Key 字符串
   */
  const std::string& WebSocketKey() const;

  /**
   * @brief 获取请求 URI
   * @return 请求 URI
   */
  const std::string& RequestUri() const;

  /**
   * @brief 设置服务器支持的子协议列表
   * @param protocols 支持的协议列表
   */
  void SetSupportedProtocols(const std::vector<std::string>& protocols);

  /**
   * @brief 获取协商后的子协议
   * @return 协商成功的协议，若未协商则为空
   */
  const std::string& NegotiatedProtocol() const;

 private:
  /**
   * @brief 生成 Sec-WebSocket-Accept 密钥
   * @param key 客户端提供的 Sec-WebSocket-Key
   * @return 计算后的 Accept 密钥
   */
  std::string GenerateAcceptKey(const std::string& key) const;

  /**
   * @brief 大小写不敏感字符串比较
   */
  static bool CaseInsensitiveStringCompare(const std::string& a,
                                           const std::string& b);

  /**
   * @brief 检查字符串列表中是否包含指定 token
   */
  static bool CaseInsensitiveContainsToken(const std::string& list,
                                           const std::string& token);

  // RFC 6455 魔数字符串
  static constexpr const char* kWebSocketMagicString =
      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  // 状态
  bool is_valid_ = false;
  HandshakeError last_error_ = HandshakeError::kNone;

  // 解析的请求数据
  std::string request_uri_;
  std::string host_;
  std::string websocket_key_;
  std::string origin_;
  std::string requested_protocols_;

  // 子协议协商
  std::vector<std::string> supported_protocols_;
  std::string negotiated_protocol_;
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_WEBSOCKET_HANDSHAKE_HANDLER_H_
