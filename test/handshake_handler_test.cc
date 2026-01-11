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
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Protocol: graphql-ws\r\n"
        "\r\n";
    
    HandshakeHandler handler;
    handler.ParseRequest(request);
    
    std::string response = handler.GenerateResponse();
    
    EXPECT_NE(response.find("Sec-WebSocket-Protocol: graphql-ws"), std::string::npos);
}
