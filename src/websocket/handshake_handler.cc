// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/handshake_handler.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <optional>
#include <sstream>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

namespace darwincore {
namespace websocket {

namespace {

// RFC 6455: 最大 header 大小限制（防止 DoS）
constexpr size_t kMaxHeaderSize = 8192;

// Base64 字符集
constexpr char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 查找表
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

bool IsValidBase64(const std::string& str) {
  if (str.empty()) return false;
  if (str.length() % 4 != 0) return false;

  for (unsigned char c : str) {
    if (c == '=') continue;
    if (kBase64Lookup[c] == -1) return false;
  }
  return true;
}

std::string Base64Encode(const unsigned char* data, size_t length) {
  std::string result;
  result.reserve((length + 2) / 3 * 4);

  for (size_t i = 0; i < length; i += 3) {
    uint32_t n = data[i] << 16;
    if (i + 1 < length) n |= data[i + 1] << 8;
    if (i + 2 < length) n |= data[i + 2];

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

  // DoS 防护：限制 header 大小
  if (request.size() > kMaxHeaderSize) {
    last_error_ = HandshakeError::kRequestTooLarge;
    return false;
  }

  // 确保请求包含完整的 CRLF 分隔符
  if (request.find("\r\n\r\n") == std::string::npos) {
    last_error_ = HandshakeError::kMalformedRequest;
    return false;
  }

  std::istringstream stream(request);
  std::string line;

  // 解析请求行
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

  if (!(request_line >> method >> request_uri_ >> http_version)) {
    last_error_ = HandshakeError::kMalformedRequest;
    return false;
  }

  // 验证 HTTP 方法必须是 GET
  if (method != "GET") {
    last_error_ = HandshakeError::kInvalidMethod;
    return false;
  }

  // 验证 HTTP/1.1
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

  // 解析请求头
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.empty()) break;

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) continue;

    std::string header_name = line.substr(0, colon_pos);
    std::string header_value = line.substr(colon_pos + 1);

    // 去除首尾空格
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

    // 使用大小写不敏感的比较
    if (CaseInsensitiveStringCompare(header_name, "Upgrade")) {
      if (CaseInsensitiveContainsToken(trimmed_value, "websocket")) {
        has_upgrade = true;
      }
    } else if (CaseInsensitiveStringCompare(header_name, "Connection")) {
      if (CaseInsensitiveContainsToken(trimmed_value, "Upgrade")) {
        has_connection = true;
      }
    } else if (CaseInsensitiveStringCompare(header_name, "Sec-WebSocket-Key")) {
      websocket_key_ = trimmed_value;
      has_websocket_key = true;
    } else if (CaseInsensitiveStringCompare(header_name, "Sec-WebSocket-Version")) {
      has_websocket_version = true;
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

  // RFC 6455 强制校验
  if (!has_upgrade) {
    last_error_ = HandshakeError::kMissingUpgrade;
    return false;
  }
  if (!has_connection) {
    last_error_ = HandshakeError::kMissingConnection;
    return false;
  }
  if (host_.empty()) {
    last_error_ = HandshakeError::kMissingHost;
    return false;
  }
  if (!has_websocket_version) {
    last_error_ = HandshakeError::kMissingVersion;
    return false;
  }
  if (!version_is_13) {
    last_error_ = HandshakeError::kInvalidVersion;
    return false;
  }
  if (!has_websocket_key || websocket_key_.empty()) {
    last_error_ = HandshakeError::kMissingKey;
    return false;
  }

  // 验证 Sec-WebSocket-Key 必须是有效的 Base64
  if (!IsValidBase64(websocket_key_)) {
    last_error_ = HandshakeError::kInvalidKey;
    return false;
  }

  // 子协议协商
  if (!requested_protocols_.empty()) {
    std::vector<std::string> client_protocols;
    std::istringstream protocol_stream(requested_protocols_);
    std::string protocol;

    while (std::getline(protocol_stream, protocol, ',')) {
      size_t start = 0;
      while (start < protocol.size() &&
             (protocol[start] == ' ' || protocol[start] == '\t')) {
        start++;
      }
      size_t end = protocol.size();
      while (end > start &&
             (protocol[end - 1] == ' ' || protocol[end - 1] == '\t')) {
        end--;
      }
      if (start < end) {
        client_protocols.push_back(protocol.substr(start, end - start));
      }
    }

    for (const auto& supported : supported_protocols_) {
      for (const auto& requested : client_protocols) {
        if (requested == supported) {
          negotiated_protocol_ = supported;
          break;
        }
      }
      if (!negotiated_protocol_.empty()) break;
    }
  }

  is_valid_ = true;
  return true;
}

std::string HandshakeHandler::GenerateResponse() const {
  if (!is_valid_) return "";

  std::string accept_key = GenerateAcceptKey(websocket_key_);

  std::ostringstream response;
  response << "HTTP/1.1 101 Switching Protocols\r\n";
  response << "Upgrade: websocket\r\n";
  response << "Connection: Upgrade\r\n";
  response << "Sec-WebSocket-Accept: " << accept_key << "\r\n";

  if (!negotiated_protocol_.empty()) {
    response << "Sec-WebSocket-Protocol: " << negotiated_protocol_ << "\r\n";
  }

  response << "\r\n";
  return response.str();
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

bool HandshakeHandler::IsValid() const { return is_valid_; }

HandshakeError HandshakeHandler::GetLastError() const { return last_error_; }

const std::string& HandshakeHandler::WebSocketKey() const { return websocket_key_; }

const std::string& HandshakeHandler::RequestUri() const { return request_uri_; }

void HandshakeHandler::SetSupportedProtocols(const std::vector<std::string>& protocols) {
  supported_protocols_ = protocols;
}

const std::string& HandshakeHandler::NegotiatedProtocol() const { return negotiated_protocol_; }

std::optional<std::pair<size_t, std::string>> HandshakeHandler::TryConsume(
    const std::vector<uint8_t>& buffer) {
  // 查找 HTTP 头结束位置 (\r\n\r\n)
  static constexpr uint8_t kEnd[] = {'\r', '\n', '\r', '\n'};

  auto it = std::search(buffer.begin(), buffer.end(),
                        std::begin(kEnd), std::end(kEnd));
  if (it == buffer.end()) {
    // 请求不完整
    return std::nullopt;
  }

  size_t http_len = static_cast<size_t>(it - buffer.begin()) + 4;  // +4 for \r\n\r\n

  // 转换为字符串进行解析
  std::string request(buffer.begin(), buffer.begin() + http_len);

  // 解析握手请求
  if (!ParseRequest(request)) {
    // 请求无效，返回 {consumed, error_response}
    return std::make_pair(http_len, GenerateErrorResponse(last_error_));
  }

  // 请求有效，返回 {consumed, success_response}
  return std::make_pair(http_len, GenerateResponse());
}

#ifdef __APPLE__
std::string HandshakeHandler::GenerateAcceptKey(const std::string& key) const {
  std::string to_hash = key + kWebSocketMagicString;
  unsigned char hash[CC_SHA1_DIGEST_LENGTH];
  CC_SHA1(to_hash.data(), static_cast<CC_LONG>(to_hash.length()), hash);
  return Base64Encode(hash, CC_SHA1_DIGEST_LENGTH);
}
#else
std::string HandshakeHandler::GenerateAcceptKey(const std::string& key) const {
  std::string to_hash = key + kWebSocketMagicString;
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(to_hash.c_str()),
       to_hash.length(), hash);
  std::string result;
  result.resize(((SHA_DIGEST_LENGTH + 2) / 3) * 4);
  EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&result[0]), hash, SHA_DIGEST_LENGTH);
  return result;
}
#endif

bool HandshakeHandler::CaseInsensitiveStringCompare(const std::string& a,
                                                     const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

bool HandshakeHandler::CaseInsensitiveContainsToken(const std::string& list,
                                                     const std::string& token) {
  std::istringstream stream(list);
  std::string item;
  while (stream >> item) {
    if (CaseInsensitiveStringCompare(item, token)) return true;
  }
  return false;
}

}  // namespace websocket
}  // namespace darwincore
