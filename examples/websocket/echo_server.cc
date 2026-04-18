// Copyright (C) 2025
// Licensed under the MIT License

// WebSocket 回显服务器示例
// 演示如何使用 WebSocketServer 处理连接和帧

#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

#include <darwincore/websocket/websocket_server.h>

using namespace darwincore::websocket;

volatile sig_atomic_t g_running = true;

void SignalHandler(int) { g_running = 0; }

int main() {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGPIPE, SIG_IGN);

    std::cout << "========================================" << std::endl;
    std::cout << "  WebSocket Echo Server (WebSocketServer)" << std::endl;
    std::cout << "========================================" << std::endl;

    WebSocketServer server;

    // 连接建立回调
    server.SetOnConnected([](const ConnectionPtr& conn) {
        std::cout << "[+] Connected: " << conn->connection_id()
                  << " from " << conn->remote_address() << std::endl;
    });

    // 帧处理回调
    server.SetOnFrame([&server](const ConnectionPtr& conn, const Frame& frame) {
        switch (frame.opcode) {
            case OpCode::kText:
            case OpCode::kBinary: {
                // 回显原始数据
                server.SendFrame(conn, frame.payload, frame.opcode);
                std::cout << "[ECHO] " << frame.payload.size() << " bytes" << std::endl;
                break;
            }
            case OpCode::kPing: {
                // 自动 Pong 由 WebSocketServer 处理
                // 这里演示如何手动处理
                server.SendPong(conn, frame.payload);
                std::cout << "[PING] -> [PONG]" << std::endl;
                break;
            }
            default:
                break;
        }
    });

    // 连接断开回调
    server.SetOnDisconnected([](const ConnectionPtr& conn) {
        std::cout << "[-] Disconnected: " << conn->connection_id() << std::endl;
    });

    // 错误回调
    server.SetOnError([](const ConnectionPtr& conn, const std::string& error) {
        std::cerr << "[!] Error on " << conn->connection_id()
                  << ": " << error << std::endl;
    });

    // 启动服务器
    if (!server.Start("127.0.0.1", 9999)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "\nServer listening on ws://127.0.0.1:9999" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    std::cout << "Active connections: " << server.GetConnectionCount() << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 定期打印连接数
        static int tick = 0;
        if (++tick % 50 == 0) {
            std::cout << "[INFO] Active connections: "
                      << server.GetConnectionCount() << std::endl;
        }
    }

    std::cout << "\nShutting down..." << std::endl;
    server.Stop();

    std::cout << "Server stopped." << std::endl;
    return 0;
}
