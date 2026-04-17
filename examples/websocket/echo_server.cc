// Copyright (C) 2025
// Licensed under the MIT License

// 纯 WebSocket 回显服务器示例
// 只处理 WebSocket 帧，不涉及 JSON-RPC

#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <darwincore/network/server.h>
#include <darwincore/websocket/frame_parser.h>
#include <darwincore/websocket/frame_builder.h>

struct ConnectionContext {
    std::unique_ptr<darwincore::websocket::FrameParser> parser;
    std::vector<uint8_t> recv_buffer;
};

darwincore::network::Server* g_server = nullptr;
volatile sig_atomic_t g_running = 1;

// 连接上下文表（简化版，没有 EventLoopGroup）
std::unordered_map<uint64_t, std::shared_ptr<ConnectionContext>> g_connections;
std::mutex g_connections_mutex;

void SignalHandler(int) { g_running = 0; }

int main() {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGPIPE, SIG_IGN);

    std::cout << "========================================" << std::endl;
    std::cout << "  Pure WebSocket Echo Server" << std::endl;
    std::cout << "========================================" << std::endl;

    darwincore::network::Server server;
    g_server = &server;

    using namespace darwincore::websocket;

    // 连接回调
    server.SetOnClientConnected([](const darwincore::network::ConnectionInformation& info) {
        std::cout << "[+] Connected: " << info.connection_id
                  << " from " << info.peer_address << std::endl;

        auto ctx = std::make_shared<ConnectionContext>();
        ctx->parser = std::make_unique<FrameParser>();

        std::lock_guard<std::mutex> lock(g_connections_mutex);
        g_connections[info.connection_id] = ctx;
    });

    // 消息回调
    server.SetOnMessage([](uint64_t connection_id,
                           const std::vector<uint8_t>& data) {
        std::shared_ptr<ConnectionContext> ctx;
        {
            std::lock_guard<std::mutex> lock(g_connections_mutex);
            auto it = g_connections.find(connection_id);
            if (it != g_connections.end()) {
                ctx = it->second;
            }
        }
        if (!ctx) return;

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
                case OpCode::kBinary: {
                    // 回显：发送相同数据
                    auto response = FrameBuilder::BuildFrame(
                        frame->opcode, frame->payload);
                    g_server->SendData(connection_id, response.data(), response.size());
                    std::cout << "[ECHO] " << frame->payload.size() << " bytes" << std::endl;
                    break;
                }
                case OpCode::kPing: {
                    // 回复 Pong
                    auto pong = FrameBuilder::BuildFrame(
                        OpCode::kPong, frame->payload);
                    g_server->SendData(connection_id, pong.data(), pong.size());
                    break;
                }
                case OpCode::kPong:
                    break;
                case OpCode::kClose: {
                    // 发送 Close 响应
                    auto close = FrameBuilder::CreateCloseFrame(1000, "");
                    g_server->SendData(connection_id, close.data(), close.size());
                    break;
                }
                default:
                    break;
            }
        }
    });

    // 断开回调
    server.SetOnClientDisconnected([](uint64_t connection_id) {
        std::cout << "[-] Disconnected: " << connection_id << std::endl;

        std::lock_guard<std::mutex> lock(g_connections_mutex);
        g_connections.erase(connection_id);
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
