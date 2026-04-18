// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/handshake_handler.h"

namespace {

using namespace darwincore::websocket;

constexpr const char* kValidKey = "dGhlIHNhbXBsZSBub25jZQ==";

std::string CreateValidRequest(const std::string& key = kValidKey,
                               const std::string& uri = "/") {
  return "GET " + uri + " HTTP/1.1\r\n"
         "Host: localhost:8080\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: " + key + "\r\n"
         "Sec-WebSocket-Version: 13\r\n"
         "\r\n";
}

}  // namespace

TEST(HandshakeHandler, ValidRequest) {
  HandshakeHandler handler;
  std::string request = CreateValidRequest();

  EXPECT_TRUE(handler.ParseRequest(request));
  EXPECT_TRUE(handler.IsValid());
  EXPECT_EQ(handler.GetLastError(), HandshakeError::kNone);
  EXPECT_EQ(handler.RequestUri(), "/");
  EXPECT_EQ(handler.WebSocketKey(), kValidKey);
}

TEST(HandshakeHandler, ValidRequestWithSubProtocol) {
  HandshakeHandler handler;
  handler.SetSupportedProtocols({"jsonrpc", "graphql"});

  std::string request = CreateValidRequest();
  request.insert(request.find("Sec-WebSocket-Version: 13"),
                 "Sec-WebSocket-Protocol: jsonrpc\r\n");

  EXPECT_TRUE(handler.ParseRequest(request));
  EXPECT_TRUE(handler.IsValid());
  EXPECT_EQ(handler.NegotiatedProtocol(), "jsonrpc");
}

TEST(HandshakeHandler, MissingUpgrade) {
  HandshakeHandler handler;
  std::string request = "GET / HTTP/1.1\r\n"
         "Host: localhost:8080\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: " + std::string(kValidKey) + "\r\n"
         "Sec-WebSocket-Version: 13\r\n"
         "\r\n";

  EXPECT_FALSE(handler.ParseRequest(request));
  EXPECT_FALSE(handler.IsValid());
  EXPECT_EQ(handler.GetLastError(), HandshakeError::kMissingUpgrade);
}

TEST(HandshakeHandler, MissingConnection) {
  HandshakeHandler handler;
  std::string request = "GET / HTTP/1.1\r\n"
         "Host: localhost:8080\r\n"
         "Upgrade: websocket\r\n"
         "Sec-WebSocket-Key: " + std::string(kValidKey) + "\r\n"
         "Sec-WebSocket-Version: 13\r\n"
         "\r\n";

  EXPECT_FALSE(handler.ParseRequest(request));
  EXPECT_FALSE(handler.IsValid());
  EXPECT_EQ(handler.GetLastError(), HandshakeError::kMissingConnection);
}

TEST(HandshakeHandler, MissingKey) {
  HandshakeHandler handler;
  std::string request = "GET / HTTP/1.1\r\n"
         "Host: localhost:8080\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Version: 13\r\n"
         "\r\n";

  EXPECT_FALSE(handler.ParseRequest(request));
  EXPECT_FALSE(handler.IsValid());
  EXPECT_EQ(handler.GetLastError(), HandshakeError::kMissingKey);
}

TEST(HandshakeHandler, InvalidVersion) {
  HandshakeHandler handler;
  std::string request = "GET / HTTP/1.1\r\n"
         "Host: localhost:8080\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: " + std::string(kValidKey) + "\r\n"
         "Sec-WebSocket-Version: 7\r\n"
         "\r\n";

  EXPECT_FALSE(handler.ParseRequest(request));
  EXPECT_FALSE(handler.IsValid());
  EXPECT_EQ(handler.GetLastError(), HandshakeError::kInvalidVersion);
}

TEST(HandshakeHandler, InvalidMethod) {
  HandshakeHandler handler;
  std::string request = "POST / HTTP/1.1\r\n"
         "Host: localhost:8080\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: " + std::string(kValidKey) + "\r\n"
         "Sec-WebSocket-Version: 13\r\n"
         "\r\n";

  EXPECT_FALSE(handler.ParseRequest(request));
  EXPECT_FALSE(handler.IsValid());
  EXPECT_EQ(handler.GetLastError(), HandshakeError::kInvalidMethod);
}

TEST(HandshakeHandler, GenerateResponse) {
  HandshakeHandler handler;
  std::string request = CreateValidRequest();

  ASSERT_TRUE(handler.ParseRequest(request));

  std::string response = handler.GenerateResponse();

  EXPECT_TRUE(response.find("HTTP/1.1 101 Switching Protocols") != std::string::npos);
  EXPECT_TRUE(response.find("Upgrade: websocket") != std::string::npos);
  EXPECT_TRUE(response.find("Connection: Upgrade") != std::string::npos);
  EXPECT_TRUE(response.find("Sec-WebSocket-Accept:") != std::string::npos);
}

TEST(HandshakeHandler, GenerateErrorResponse) {
  HandshakeHandler handler;

  std::string response = handler.GenerateErrorResponse(HandshakeError::kMalformedRequest);
  EXPECT_TRUE(response.find("400 Bad Request") != std::string::npos);

  response = handler.GenerateErrorResponse(HandshakeError::kInvalidVersion);
  EXPECT_TRUE(response.find("426 Upgrade Required") != std::string::npos);
  EXPECT_TRUE(response.find("Sec-WebSocket-Version: 13") != std::string::npos);
}

TEST(HandshakeHandler, RequestTooLarge) {
  HandshakeHandler handler;
  std::string request(8193, 'x');

  EXPECT_FALSE(handler.ParseRequest(request));
  EXPECT_EQ(handler.GetLastError(), HandshakeError::kRequestTooLarge);
}

TEST(HandshakeHandler, TryConsumeIncomplete) {
  HandshakeHandler handler;
  std::vector<uint8_t> incomplete = {'G', 'E', 'T', ' '};

  auto result = handler.TryConsume(incomplete);
  EXPECT_FALSE(result.has_value());
}

TEST(HandshakeHandler, TryConsumeComplete) {
  HandshakeHandler handler;
  std::string request =
      "GET / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "\r\n";
  std::vector<uint8_t> buffer(request.begin(), request.end());

  auto result = handler.TryConsume(buffer);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, request.size());
  EXPECT_TRUE(result->second.find("HTTP/1.1 101 Switching Protocols") != std::string::npos);
}

TEST(HandshakeHandler, TryConsumeWithTrailingData) {
  HandshakeHandler handler;
  std::string request =
      "GET / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "\r\n";
  std::string trailing = "WEBSOCKET_FRAME_DATA";
  std::vector<uint8_t> buffer(request.begin(), request.end());
  buffer.insert(buffer.end(), trailing.begin(), trailing.end());

  auto result = handler.TryConsume(buffer);
  ASSERT_TRUE(result.has_value());
  // 只消费 HTTP 头，保留后续 WebSocket 数据
  EXPECT_EQ(result->first, request.size());
}

TEST(HandshakeHandler, TryConsumeInvalid) {
  HandshakeHandler handler;
  std::string request =
      "POST / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";
  std::vector<uint8_t> buffer(request.begin(), request.end());

  auto result = handler.TryConsume(buffer);
  ASSERT_TRUE(result.has_value());
  // 返回错误响应
  EXPECT_TRUE(result->second.find("400 Bad Request") != std::string::npos);
}
