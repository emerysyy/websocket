// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/handshake_handler.h"

using namespace darwincore::websocket;

TEST(HandshakeHandlerTest, ParseValidRequest) {
    std::string request =
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_TRUE(result);
    EXPECT_TRUE(handler.IsValid());
    EXPECT_EQ(handler.RequestUri(), "/chat");
}

TEST(HandshakeHandlerTest, GenerateCorrectResponse) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    
    HandshakeHandler handler;
    handler.ParseRequest(request);
    
    std::string response = handler.GenerateResponse();
    
    EXPECT_NE(response.find("HTTP/1.1 101 Switching Protocols"), std::string::npos);
    EXPECT_NE(response.find("Upgrade: websocket"), std::string::npos);
    EXPECT_NE(response.find("Connection: Upgrade"), std::string::npos);
    EXPECT_NE(response.find("Sec-WebSocket-Accept:"), std::string::npos);
}

TEST(HandshakeHandlerTest, RejectMissingKey) {
    std::string request = 
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "\r\n";
    
    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(handler.IsValid());
}

TEST(HandshakeHandlerTest, RejectNonGet) {
    std::string request = 
        "POST / HTTP/1.1\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "\r\n";
    
    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);
    
    EXPECT_FALSE(result);
}

TEST(HandshakeHandlerTest, ExtractProtocol) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Protocol: graphql-ws\r\n"
        "\r\n";

    HandshakeHandler handler;
    // ⚠️ 需要设置服务器支持的协议列表
    handler.SetSupportedProtocols({"graphql-ws", "soap", "mqtt"});
    handler.ParseRequest(request);

    std::string response = handler.GenerateResponse();

    EXPECT_NE(response.find("Sec-WebSocket-Protocol: graphql-ws"), std::string::npos);
}

// ✅ 新增测试：RFC 6455 校验缺失 Upgrade 头
TEST(HandshakeHandlerTest, RejectMissingUpgrade) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
}

// ✅ 新增测试：RFC 6455 校验缺失 Connection 头
TEST(HandshakeHandlerTest, RejectMissingConnection) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
}

// ✅ 新增测试：RFC 6455 校验 Sec-WebSocket-Version 必须是 13
TEST(HandshakeHandlerTest, RejectInvalidVersion) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 12\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
}

// ✅ 新增测试：大小写不敏感的 header 名称
TEST(HandshakeHandlerTest, CaseInsensitiveHeaders) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "upgrade: websocket\r\n"
        "connection: Upgrade\r\n"
        "sec-websocket-key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "sec-websocket-version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_TRUE(result);
    EXPECT_TRUE(handler.IsValid());
}

// ✅ 新增测试：Connection 头包含多个值
TEST(HandshakeHandlerTest, ConnectionWithMultipleValues) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_TRUE(result);
    EXPECT_TRUE(handler.IsValid());
}

// ✅ 新增测试：非法的 Base64 Key
TEST(HandshakeHandlerTest, RejectInvalidBase64Key) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: not-valid-base64!@#\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
}

// ✅ 新增测试：DoS 防护 - 超大 header
TEST(HandshakeHandlerTest, RejectOversizedHeaders) {
    // 创建一个超过 8192 字节的请求
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "X-Large-Header: ";

    // 添加大量数据使请求超过 8192 字节
    request.append(10000, 'A');
    request += "\r\n\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
}

// ✅ 新增测试：Subprotocol 协商 - 客户端请求多个协议
TEST(HandshakeHandlerTest, NegotiateMultipleProtocols) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Protocol: chat, superchat, mqtt\r\n"
        "\r\n";

    HandshakeHandler handler;
    // 服务器只支持 chat 和 mqtt
    handler.SetSupportedProtocols({"mqtt", "chat"});
    handler.ParseRequest(request);

    std::string response = handler.GenerateResponse();

    // 应该选择第一个匹配的协议：mqtt
    EXPECT_NE(response.find("Sec-WebSocket-Protocol: mqtt"), std::string::npos);
    EXPECT_EQ(response.find("superchat"), std::string::npos);
}

// ✅ 新增测试：Subprotocol 协商 - 无匹配协议
TEST(HandshakeHandlerTest, NoMatchingProtocol) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Protocol: unknown-protocol\r\n"
        "\r\n";

    HandshakeHandler handler;
    // 服务器不支持客户端请求的协议
    handler.SetSupportedProtocols({"chat", "mqtt"});
    handler.ParseRequest(request);

    std::string response = handler.GenerateResponse();

    // 握手成功，但不返回 Sec-WebSocket-Protocol 头
    EXPECT_TRUE(handler.IsValid());
    EXPECT_EQ(response.find("Sec-WebSocket-Protocol:"), std::string::npos);
}

// ✅ 新增测试：错误响应生成
TEST(HandshakeHandlerTest, GenerateErrorResponse) {
    std::string response = HandshakeHandler::GenerateErrorResponse(
        HandshakeError::kInvalidVersion);

    EXPECT_NE(response.find("HTTP/1.1 426"), std::string::npos);
    EXPECT_NE(response.find("Sec-WebSocket-Version: 13"), std::string::npos);
}

// ✅ 新增测试：HTTP/1.0 被拒绝
TEST(HandshakeHandlerTest, RejectHttp10) {
    std::string request =
        "GET / HTTP/1.0\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
    EXPECT_EQ(handler.GetLastError(), HandshakeError::kInvalidHttpVersion);
}

// ✅ 新增测试：拒绝缺失 Host 头（HTTP/1.1 强制要求）
TEST(HandshakeHandlerTest, RejectMissingHost) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
    EXPECT_EQ(handler.GetLastError(), HandshakeError::kMissingHost);
}

// ✅ 新增测试：token 匹配不应该匹配子串
TEST(HandshakeHandlerTest, TokenMatchNoSubstring) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: keep-alive, Upgraded\r\n"  // 注意是 Upgraded 而不是 Upgrade
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    // Upgraded != Upgrade，应该拒绝
    EXPECT_FALSE(result);
    EXPECT_EQ(handler.GetLastError(), HandshakeError::kMissingConnection);
}

// ✅ 新增测试：不完整的请求（缺少 CRLF 终结符）
TEST(HandshakeHandlerTest, RejectIncompleteRequest) {
    std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n";  // 缺少 \r\n\r\n

    HandshakeHandler handler;
    bool result = handler.ParseRequest(request);

    EXPECT_FALSE(result);
    EXPECT_EQ(handler.GetLastError(), HandshakeError::kMalformedRequest);
}

