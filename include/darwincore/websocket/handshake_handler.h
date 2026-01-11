// Copyright (C) 2025
// Licensed under the MIT License

#pragma once

#include <string>

namespace darwincore {
namespace websocket {

/**
 * @brief WebSocket 握手处理器
 * 
 * 负责解析 HTTP 升级请求并生成 WebSocket 握手响应。
 * 遵循 RFC 6455 标准。
 */
class HandshakeHandler {
public:
    /**
     * @brief 默认构造函数
     */
    HandshakeHandler() = default;
    
    /**
     * @brief 解析 HTTP 升级请求
     * @param request HTTP 请求字符串
     * @return 解析成功返回 true
     */
    bool ParseRequest(const std::string& request);
    
    /**
     * @brief 生成 HTTP 升级响应
     * @return HTTP 响应字符串
     */
    std::string GenerateResponse() const;
    
    /**
     * @brief 验证握手是否有效
     * @return 有效返回 true
     */
    bool IsValid() const;
    
    /**
     * @brief 获取提取的 WebSocket 密钥
     * @return WebSocket 密钥
     */
    const std::string& WebSocketKey() const;
    
    /**
     * @brief 获取请求的 URI
     * @return 请求 URI
     */
    const std::string& RequestUri() const;

private:
    /**
     * @brief 生成 Sec-WebSocket-Accept 值
     * @param key Sec-WebSocket-Key
     * @return Sec-WebSocket-Accept 值
     */
    std::string GenerateAcceptKey(const std::string& key) const;
    
    static constexpr const char* kWebSocketMagicString =
        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    std::string request_uri_;
    std::string host_;
    std::string websocket_key_;
    std::string origin_;
    std::string protocol_;
    bool is_valid_ = false;
};

}  // namespace websocket
}  // namespace darwincore
