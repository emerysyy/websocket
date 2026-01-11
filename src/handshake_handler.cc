// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/handshake_handler.h"

#include <cstring>
#include <sstream>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#include <openssl/base64.h>
#endif

namespace darwincore {
namespace websocket {

bool HandshakeHandler::ParseRequest(const std::string& request) {
    is_valid_ = false;
    
    std::istringstream stream(request);
    std::string line;
    
    // 解析请求行
    if (!std::getline(stream, line)) {
        return false;
    }
    
    // 移除末尾的 \r
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream request_line(line);
    std::string method, http_version;
    request_line >> method >> request_uri_ >> http_version;
    
    if (method != "GET") {
        return false;
    }
    
    // 解析请求头
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
            continue;
        }
        
        std::string header_name = line.substr(0, colon_pos);
        std::string header_value = line.substr(colon_pos + 1);
        
        // 去除首尾空格
        while (!header_value.empty() && header_value.front() == ' ') {
            header_value.erase(header_value.begin());
        }
        while (!header_value.empty() && header_value.back() == ' ') {
            header_value.pop_back();
        }
        
        if (header_name == "Host") {
            host_ = header_value;
        } else if (header_name == "Sec-WebSocket-Key") {
            websocket_key_ = header_value;
        } else if (header_name == "Origin") {
            origin_ = header_value;
        } else if (header_name == "Sec-WebSocket-Protocol") {
            protocol_ = header_value;
        }
    }
    
    // 验证必需的头部
    if (websocket_key_.empty()) {
        return false;
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
    
    if (!protocol_.empty()) {
        response << "Sec-WebSocket-Protocol: " << protocol_ << "\r\n";
    }
    
    response << "\r\n";
    
    return response.str();
}

bool HandshakeHandler::IsValid() const {
    return is_valid_;
}

const std::string& HandshakeHandler::WebSocketKey() const {
    return websocket_key_;
}

const std::string& HandshakeHandler::RequestUri() const {
    return request_uri_;
}

// 手写 Base64 编码表
namespace {
const char kBase64Chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
    
    // Base64 编码
    std::string result;
    result.resize(EVP_EncodeBlock(nullptr, hash, SHA_DIGEST_LENGTH));
    EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(&result[0]),
        hash,
        SHA_DIGEST_LENGTH);
    
    return result;
}
#endif

}  // namespace websocket
}  // namespace darwincore
