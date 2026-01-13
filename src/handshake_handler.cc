// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/handshake_handler.h"

#include <cstring>
#include <sstream>
#include <vector>
#include <regex>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#include <openssl/base64.h>
#endif

namespace darwincore {
namespace websocket {

namespace {

// RFC 6455: 最大 header 大小限制（防止 DoS）
constexpr size_t kMaxHeaderSize = 8192;

// Base64 字符集
constexpr char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// ⚠️ 性能优化：256 字节查找表，O(1) Base64 字符验证
// -1: 无效字符
// 0-63: Base64 字符对应的值
constexpr int8_t kBase64Lookup[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0x00-0x0F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0x10-0x1F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,  // 0x20-0x2F (+, /)
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,  // 0x30-0x3F (0-9)
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  // 0x40-0x4F (A-O)
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,  // 0x50-0x5F (P-Z)
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  // 0x60-0x6F (a-o)
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,  // 0x70-0x7F (p-z)
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0x80-0x8F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0x90-0x9F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0xA0-0xAF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0xB0-0xBF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0xC0-0xCF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0xD0-0xDF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0xE0-0xEF
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1   // 0xF0-0xFF
};

/**
 * @brief 快速验证 Base64 字符串格式（使用查找表优化）
 * @param str 待验证的字符串
 * @return 如果是有效的 Base64 字符串返回 true
 */
bool IsValidBase64(const std::string& str) {
    if (str.empty()) {
        return false;
    }

    // Base64 编码后长度必须是 4 的倍数
    if (str.length() % 4 != 0) {
        return false;
    }

    size_t padding = 0;
    for (auto it = str.rbegin(); it != str.rend() && (*it == '='); ++it) {
        padding++;
    }

    // Base64 最多只能有 2 个 padding 字符
    if (padding > 2) {
        return false;
    }

    // ⚠️ 性能优化：使用查找表进行 O(1) 验证，而不是 O(64*n) 查表
    for (unsigned char c : str) {
        if (c == '=') {
            continue;
        }
        if (kBase64Lookup[c] == -1) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Base64 解码（使用查找表优化）
 * @param encoded Base64 编码的字符串
 * @param out 解码后的数据
 * @return 解码成功返回 true
 */
bool Base64Decode(const std::string& encoded, std::vector<unsigned char>& out) {
    if (!IsValidBase64(encoded)) {
        return false;
    }

    out.clear();
    out.reserve(encoded.length() * 3 / 4);

    uint32_t buffer = 0;
    int bits = 0;

    for (unsigned char c : encoded) {
        if (c == '=') {
            break;
        }

        // ⚠️ 性能优化：使用查找表 O(1) 获取值，而不是循环查找
        int value = kBase64Lookup[c];
        if (value == -1) {
            return false;
        }

        buffer = (buffer << 6) | static_cast<uint32_t>(value);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buffer >> bits) & 0xFF));
        }
    }

    return true;
}

}  // namespace

bool HandshakeHandler::ParseRequest(const std::string& request) {
    // 重置状态
    is_valid_ = false;
    last_error_ = HandshakeError::kNone;
    request_uri_.clear();
    host_.clear();
    websocket_key_.clear();
    origin_.clear();
    requested_protocols_.clear();
    negotiated_protocol_.clear();

    // ✅ 5. DoS 防护：限制 header 大小
    if (request.size() > kMaxHeaderSize) {
        last_error_ = HandshakeError::kRequestTooLarge;
        return false;
    }

    // ❗ 问题 1 修复：确保请求包含完整的 CRLF 分隔符（防止 TCP 半包解析）
    // HTTP 请求必须以 \r\n\r\n 结束，标记 header 部分的结束
    if (request.find("\r\n\r\n") == std::string::npos) {
        last_error_ = HandshakeError::kMalformedRequest;
        return false;
    }

    std::istringstream stream(request);
    std::string line;

    // ✅ 6. 解析并验证请求行格式完整性
    if (!std::getline(stream, line)) {
        last_error_ = HandshakeError::kMalformedRequest;
        return false;
    }

    // 移除末尾的 \r
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::istringstream request_line(line);
    std::string method, http_version;

    // 验证请求行必须有 3 个字段
    if (!(request_line >> method >> request_uri_ >> http_version)) {
        last_error_ = HandshakeError::kMalformedRequest;
        return false;
    }

    // 验证 HTTP 方法必须是 GET
    if (method != "GET") {
        last_error_ = HandshakeError::kInvalidMethod;
        return false;
    }

    // ⚠️ RFC 6455 明确要求：The request MUST be an HTTP/1.1 request
    if (http_version != "HTTP/1.1") {
        last_error_ = HandshakeError::kInvalidHttpVersion;
        return false;
    }

    // RFC 6455 必需的 header 标志
    bool has_upgrade = false;
    bool has_connection = false;
    bool has_websocket_key = false;
    bool has_websocket_version = false;
    bool version_is_13 = false;

    // ✅ 3. 解析请求头（大小写不敏感的 header 名称）
    while (std::getline(stream, line)) {
        // 移除末尾的 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            break;  // 到达空行，表示头部结束
        }

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;  // 跳过格式错误的 header
        }

        std::string header_name = line.substr(0, colon_pos);
        std::string header_value = line.substr(colon_pos + 1);

        // 去除首尾空格（优化：避免 O(n) 移动）
        size_t value_start = 0;
        while (value_start < header_value.size() &&
               (header_value[value_start] == ' ' || header_value[value_start] == '\t')) {
            value_start++;
        }

        size_t value_end = header_value.size();
        while (value_end > value_start &&
               (header_value[value_end - 1] == ' ' || header_value[value_end - 1] == '\t')) {
            value_end--;
        }

        std::string trimmed_value = header_value.substr(value_start, value_end - value_start);

        // ✅ 3. 使用大小写不敏感的比较
        if (CaseInsensitiveStringCompare(header_name, "Upgrade")) {
            // ⚠️ RFC 6455: Upgrade header 可能包含多个值（如 "websocket, foo"）
            // 必须包含 "websocket" token（大小写不敏感）
            if (CaseInsensitiveContainsToken(trimmed_value, "websocket")) {
                has_upgrade = true;
            }
        } else if (CaseInsensitiveStringCompare(header_name, "Connection")) {
            // ✅ 1. 校验 Connection 头必须包含 "Upgrade"（可能是列表）
            if (CaseInsensitiveContainsToken(trimmed_value, "Upgrade")) {
                has_connection = true;
            }
        } else if (CaseInsensitiveStringCompare(header_name, "Sec-WebSocket-Key")) {
            websocket_key_ = trimmed_value;
            has_websocket_key = true;
        } else if (CaseInsensitiveStringCompare(header_name, "Sec-WebSocket-Version")) {
            has_websocket_version = true;
            // ✅ 2. 校验 Sec-WebSocket-Version 必须是 13
            // RFC 6455 的强制要求
            if (trimmed_value == "13") {
                version_is_13 = true;
            }
        } else if (CaseInsensitiveStringCompare(header_name, "Host")) {
            host_ = trimmed_value;
        } else if (CaseInsensitiveStringCompare(header_name, "Origin")) {
            origin_ = trimmed_value;
        } else if (CaseInsensitiveStringCompare(header_name, "Sec-WebSocket-Protocol")) {
            requested_protocols_ = trimmed_value;
        }
    }

    // ✅ 1. RFC 6455 强制要求：必须有 Upgrade 和 Connection 头
    if (!has_upgrade) {
        last_error_ = HandshakeError::kMissingUpgrade;
        return false;
    }
    if (!has_connection) {
        last_error_ = HandshakeError::kMissingConnection;
        return false;
    }

    // ❗ 问题 2 修复：RFC 7230 强制要求 HTTP/1.1 请求必须包含 Host 头
    if (host_.empty()) {
        last_error_ = HandshakeError::kMissingHost;
        return false;
    }

    // ✅ 2. RFC 6455 强制要求：必须有 Sec-WebSocket-Version 头且必须为 13
    if (!has_websocket_version) {
        last_error_ = HandshakeError::kMissingVersion;
        return false;
    }
    if (!version_is_13) {
        last_error_ = HandshakeError::kInvalidVersion;
        return false;
    }

    // ✅ 4. RFC 6455 强制要求：必须有 Sec-WebSocket-Key 头
    if (!has_websocket_key || websocket_key_.empty()) {
        last_error_ = HandshakeError::kMissingKey;
        return false;
    }

    // ✅ 4. 验证 Sec-WebSocket-Key 必须是有效的 Base64
    if (!IsValidBase64(websocket_key_)) {
        last_error_ = HandshakeError::kInvalidKey;
        return false;
    }

    // ✅ 4. 验证 Sec-WebSocket-Key 解码后必须是 16 字节
    std::vector<unsigned char> decoded_key;
    if (!Base64Decode(websocket_key_, decoded_key)) {
        last_error_ = HandshakeError::kInvalidKey;
        return false;
    }

    if (decoded_key.size() != 16) {
        last_error_ = HandshakeError::kInvalidKey;
        return false;
    }

    // ✅ 7. Origin 校验（可选，但强烈建议）
    // 如果需要启用 Origin 校验，可以在这里添加白名单检查
    // 例如：if (!IsOriginAllowed(origin_)) { return false; }

    // ⚠️ Subprotocol 协商：RFC 6455 要求从客户端请求的协议中选择一个服务器支持的
    if (!requested_protocols_.empty()) {
        // 解析客户端请求的协议列表（逗号分隔）
        std::vector<std::string> client_protocols;
        std::istringstream protocol_stream(requested_protocols_);
        std::string protocol;

        while (std::getline(protocol_stream, protocol, ',')) {
            // 去除空格
            size_t start = 0;
            while (start < protocol.size() && (protocol[start] == ' ' || protocol[start] == '\t')) {
                start++;
            }
            size_t end = protocol.size();
            while (end > start && (protocol[end - 1] == ' ' || protocol[end - 1] == '\t')) {
                end--;
            }
            if (start < end) {
                client_protocols.push_back(protocol.substr(start, end - start));
            }
        }

        // 从服务器支持的协议列表中选择第一个匹配的
        for (const auto& supported : supported_protocols_) {
            for (const auto& requested : client_protocols) {
                if (requested == supported) {
                    negotiated_protocol_ = supported;
                    break;
                }
            }
            if (!negotiated_protocol_.empty()) {
                break;
            }
        }

        // RFC 6455: 如果客户端请求了协议但服务器不支持任何请求的协议，
        // 则握手失败（不返回 Sec-WebSocket-Protocol 头）
        // 这里我们允许继续握手，但不返回协议头
    }

    is_valid_ = true;
    return true;
}

std::string HandshakeHandler::GenerateResponse() const {
    if (!is_valid_) {
        return "";
    }

    std::string accept_key = GenerateAcceptKey(websocket_key_);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << accept_key << "\r\n";

    // ⚠️ Subprotocol 协商：只返回成功协商的协议
    if (!negotiated_protocol_.empty()) {
        response << "Sec-WebSocket-Protocol: " << negotiated_protocol_ << "\r\n";
    }

    response << "\r\n";

    return response.str();
}

bool HandshakeHandler::IsValid() const {
    return is_valid_;
}

HandshakeError HandshakeHandler::GetLastError() const {
    return last_error_;
}

std::string HandshakeHandler::GenerateErrorResponse(HandshakeError error) {
    std::ostringstream response;
    response << "HTTP/1.1 ";

    switch (error) {
        case HandshakeError::kInvalidHttpVersion:
        case HandshakeError::kInvalidMethod:
        case HandshakeError::kMalformedRequest:
            response << "400 Bad Request\r\n";
            break;
        case HandshakeError::kInvalidVersion:
            // RFC 6455: 当版本不支持时，应返回 426 并列出支持的版本
            response << "426 Upgrade Required\r\n";
            response << "Sec-WebSocket-Version: 13\r\n";
            break;
        case HandshakeError::kRequestTooLarge:
            response << "431 Request Header Fields Too Large\r\n";
            break;
        default:
            response << "400 Bad Request\r\n";
            break;
    }

    response << "\r\n";
    return response.str();
}

const std::string& HandshakeHandler::WebSocketKey() const {
    return websocket_key_;
}

const std::string& HandshakeHandler::RequestUri() const {
    return request_uri_;
}

void HandshakeHandler::SetSupportedProtocols(const std::vector<std::string>& protocols) {
    supported_protocols_ = protocols;
}

const std::string& HandshakeHandler::NegotiatedProtocol() const {
    return negotiated_protocol_;
}

namespace {

std::string Base64Encode(const unsigned char* data, size_t length) {
    std::string result;
    result.reserve((length + 2) / 3 * 4);

    for (size_t i = 0; i < length; i += 3) {
        uint32_t n = data[i] << 16;
        if (i + 1 < length) {
            n |= data[i + 1] << 8;
        }
        if (i + 2 < length) {
            n |= data[i + 2];
        }

        result.push_back(kBase64Chars[(n >> 18) & 0x3F]);
        result.push_back(kBase64Chars[(n >> 12) & 0x3F]);

        if (i + 1 < length) {
            result.push_back(kBase64Chars[(n >> 6) & 0x3F]);
        } else {
            result.push_back('=');
        }

        if (i + 2 < length) {
            result.push_back(kBase64Chars[n & 0x3F]);
        } else {
            result.push_back('=');
        }
    }

    return result;
}

}  // namespace

#ifdef __APPLE__
std::string HandshakeHandler::GenerateAcceptKey(const std::string& key) const {
    // 拼接 key 和 magic string
    std::string to_hash = key + kWebSocketMagicString;
    
    // SHA1 哈希 (CommonCrypto)
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    CC_SHA1(to_hash.data(), static_cast<CC_LONG>(to_hash.length()), hash);
    
    // Base64 编码
    return Base64Encode(hash, CC_SHA1_DIGEST_LENGTH);
}
#else
std::string HandshakeHandler::GenerateAcceptKey(const std::string& key) const {
    // 拼接 key 和 magic string
    std::string to_hash = key + kWebSocketMagicString;

    // SHA1 哈希 (OpenSSL)
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(to_hash.c_str()),
         to_hash.length(), hash);

    // ❗ 问题 3 修复：EVP_EncodeBlock(nullptr, ...) 在某些 OpenSSL 版本会 crash
    // 必须预先计算 buffer 大小：Base64 编码后长度为 (n + 2) / 3 * 4
    std::string result;
    result.resize(((SHA_DIGEST_LENGTH + 2) / 3) * 4);
    EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(&result[0]),
        hash,
        SHA_DIGEST_LENGTH);

    return result;
}
#endif

}  // namespace websocket
}  // namespace darwincore
