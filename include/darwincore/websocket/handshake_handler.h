// Copyright (C) 2025
// Licensed under the MIT License

#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace darwincore {
namespace websocket {

// 辅助函数：大小写不敏感的字符串比较
inline bool CaseInsensitiveStringCompare(const std::string& str1, const std::string& str2) {
    if (str1.length() != str2.length()) {
        return false;
    }
    return std::equal(str1.begin(), str1.end(), str2.begin(),
        [](char a, char b) {
            return tolower(static_cast<unsigned char>(a)) ==
                   tolower(static_cast<unsigned char>(b));
        });
}

// 辅助函数：检查字符串中是否包含某个 token（大小写不敏感，用于 Connection 头等）
inline bool CaseInsensitiveContainsToken(const std::string& header_value, const std::string& token) {
    auto it = header_value.begin();
    auto end = header_value.end();

    while (it != end) {
        // 跳过逗号和空格
        while (it != end && (*it == ',' || *it == ' ' || *it == '\t')) {
            ++it;
        }

        auto token_start = it;
        // 找到 token 结束位置
        while (it != end && *it != ',' && *it != ' ' && *it != '\t') {
            ++it;
        }

        std::string found_token(token_start, it);
        if (CaseInsensitiveStringCompare(found_token, token)) {
            return true;
        }
    }

    return false;
}

/**
 * @brief WebSocket 握手错误码
 */
enum class HandshakeError {
    kNone = 0,
    kInvalidHttpVersion,      // HTTP 版本不是 1.1
    kInvalidMethod,           // 不是 GET 方法
    kMissingUpgrade,          // 缺少 Upgrade 头
    kMissingConnection,       // 缺少 Connection 头
    kMissingVersion,          // 缺少 Sec-WebSocket-Version 头
    kInvalidVersion,          // Sec-WebSocket-Version 不是 13
    kMissingKey,              // 缺少 Sec-WebSocket-Key 头
    kInvalidKey,              // Sec-WebSocket-Key 格式无效
    kMissingHost,             // 缺少 Host 头（HTTP/1.1 强制要求）
    kRequestTooLarge,         // 请求过大（DoS 防护）
    kMalformedRequest         // 请求格式错误
};

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
     * @brief 生成 HTTP 错误响应
     * @param error 错误码
     * @return HTTP 错误响应字符串
     */
    static std::string GenerateErrorResponse(HandshakeError error);

    /**
     * @brief 验证握手是否有效
     * @return 有效返回 true
     */
    bool IsValid() const;

    /**
     * @brief 获取最后的错误码
     * @return 错误码
     */
    HandshakeError GetLastError() const;

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

    /**
     * @brief 设置服务器支持的子协议列表
     * @param protocols 服务器支持的协议列表
     *
     * 用于协商 Subprotocol。如果客户端提供了 Sec-WebSocket-Protocol，
     * 服务器将从此列表中选择第一个匹配的协议。
     */
    void SetSupportedProtocols(const std::vector<std::string>& protocols);

    /**
     * @brief 获取协商后的子协议
     * @return 协商后的协议，如果没有则为空
     */
    const std::string& NegotiatedProtocol() const;

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
    std::string requested_protocols_;  // 客户端请求的协议列表
    std::string negotiated_protocol_;  // 协商后的协议
    std::vector<std::string> supported_protocols_;  // 服务器支持的协议列表
    bool is_valid_ = false;
    HandshakeError last_error_ = HandshakeError::kNone;
};

}  // namespace websocket
}  // namespace darwincore
