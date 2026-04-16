// Copyright (C) 2025
// Licensed under the MIT License

// 纯 WebSocket 回显服务器示例
// 只处理 WebSocket 帧，不涉及 JSON-RPC

#include <csignal>
#include <iostream>
#include <thread>

#include "darwincore/network/server.h"
#include "darwincore/websocket/frame_parser.h"
#include "darwincore/websocket/frame_builder.h"

using namespace darwincore::websocket;

struct ConnectionContext {
    std::unique_ptr<FrameParser> parser;
    bool connected = false;
};

DarwinCore::Server* g_server = nullptr;
volatile sig_atomic_t g_running = 1;

void SignalHandler(int) { g_running = 0; }

int main() {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGPIPE, SIG_IGN);

    std::cout << "========================================" << std::endl;
    std::cout << "  Pure WebSocket Echo Server" << std::endl;
    std::cout << "========================================" << std::endl;

    DarwinCore::Server server;
    g_server = &server;

    // 连接回调
    server.SetOnClientConnected([](const DarwinCore::ConnectionInformation& info) {
        std::cout << "[+] Connected: " << info.connection_id
                  << " from " << info.peer_address << ":" << info.peer_port
                  << std::endl;

        auto ctx = std::make_shared<ConnectionContext>();
        ctx->parser = std::make_unique<FrameParser>();
        ctx->connected = true;
        info.connection->SetContext(ctx);
    });

    server.SetOnClientDisconnected([](const DarwinCore::ConnectionInformation& info) {
        std::cout << "[-] Disconnected: " << info.connection_id << std::endl;
    });

    // 消息回调
    server.SetOnMessage([](const DarwinCore::ConnectionInformation& info,
                           const std::vector<uint8_t>& data) {
        auto ctx = std::static_pointer_cast<ConnectionContext>(
            info.connection->GetContext());
        if (!ctx || !ctx->connected) return;

        // 追加到缓冲区
        ctx->recv_buffer.insert(ctx->recv_buffer.end(), data.begin(), data.end());

        // 循环解析帧
        while (true) {
            size_t consumed = 0;
            auto frame = ctx->parser->Parse(ctx->recv_buffer, consumed);

            if (!frame) break;

            // 移除已处理的字节
            ctx->recv_buffer.erase(ctx->recv_buffer.begin(),
                                   ctx->recv_buffer.begin() + consumed);

            // 处理帧
            switch (frame->opcode) {
                case OpCode::kText:
                case OpCode::kBinary:
                    // 回显：发送相同数据
                    info.connection->Send(frame->payload.data(),
                                           frame->payload.size());
                    std::cout << "[ECHO] " << frame->payload.size() << " bytes" << std::endl;
                    break;

                case OpCode::kPing:
                    // 回复 Pong
                    info.connection->Send(FrameBuilder::CreatePongFrame(frame->payload));
                    break;

                case OpCode::kPong:
                    // 不需要处理
                    break;

                case OpCode::kClose:
                    // 发送 Close 响应并关闭
                    info.connection->Send(FrameBuilder::CreateCloseFrame());
                    info.connection->Close();
                    break;

                default:
                    break;
            }
        }
    });

    // 启动服务器
    if (!server.StartIPv4("127.0.0.1", 9999)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "\nServer listening on ws://127.0.0.1:9999" << std::endl;
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down..." << std::endl;
    server.Stop();

    return 0;
}