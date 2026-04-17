// Copyright (C) 2025
// Licensed under the MIT License

// 纯 WebSocket 回显服务器示例
// 演示如何处理 HTTP Upgrade 握手，然后处理 WebSocket 帧

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
#include <darwincore/websocket/handshake_handler.h>

struct ConnectionContext {
    std::unique_ptr<darwincore::websocket::FrameParser> parser;
    std::vector<uint8_t> recv_buffer;
    bool handshake_completed = false;  // 标记握手是否完成
};

darwincore::network::Server* g_server = nullptr;
volatile sig_atomic_t g_running = 1;

std::unordered_map<uint64_t, std::shared_ptr<ConnectionContext>> g_connections;
std::mutex g_connections_mutex;

void SignalHandler(int) { g_running = 0; }

// 查找 HTTP 请求的结束位置（\r\n\r\n）
size_t FindHttpEnd(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 4) return std::string::npos;
    for (size_t i = 0; i + 3 < buffer.size(); ++i) {
        if (buffer[i] == '\r' && buffer[i+1] == '\n' &&
            buffer[i+2] == '\r' && buffer[i+3] == '\n') {
            return i + 4;
        }
    }
    return std::string::npos;
}

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

    // 连接建立 - 初始化上下文
    server.SetOnClientConnected([](const darwincore::network::ConnectionInformation& info) {
        std::cout << "[+] Connected: " << info.connection_id
                  << " from " << info.peer_address << std::endl;

        auto ctx = std::make_shared<ConnectionContext>();
        ctx->parser = std::make_unique<FrameParser>();

        std::lock_guard<std::mutex> lock(g_connections_mutex);
        g_connections[info.connection_id] = ctx;
    });

    // 接收数据 - 处理握手或 WebSocket 帧
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

        // ========== 阶段 1: 处理 HTTP Upgrade 握手 ==========
        if (!ctx->handshake_completed) {
            // 查找 HTTP 头结束位置
            size_t http_end = FindHttpEnd(ctx->recv_buffer);
            if (http_end == std::string::npos) {
                // HTTP 请求还不完整，等待更多数据
                return;
            }

            // 转换为字符串进行握手处理
            std::string http_request(ctx->recv_buffer.begin(),
                                     ctx->recv_buffer.begin() + http_end);

            // 解析握手请求
            HandshakeHandler handler;
            if (!handler.ParseRequest(http_request)) {
                std::cerr << "[!] Invalid handshake from " << connection_id << std::endl;
                auto error_resp = handler.GenerateErrorResponse(handler.GetLastError());
                g_server->SendData(connection_id,
                                   reinterpret_cast<const uint8_t*>(error_resp.data()),
                                   error_resp.size());
                return;
            }

            // 生成握手响应
            std::string response = handler.GenerateResponse();

            // 发送握手响应
            g_server->SendData(connection_id,
                               reinterpret_cast<const uint8_t*>(response.data()),
                               response.size());

            // 移除已处理的 HTTP 数据
            ctx->recv_buffer.erase(ctx->recv_buffer.begin(),
                                   ctx->recv_buffer.begin() + http_end);
            ctx->handshake_completed = true;

            std::cout << "[+] Handshake completed: " << connection_id << std::endl;
            return;
        }

        // ========== 阶段 2: 处理 WebSocket 帧 ==========
        while (!ctx->recv_buffer.empty()) {
            size_t consumed = 0;
            auto frame = ctx->parser->Parse(ctx->recv_buffer, consumed);

            if (!frame) break;

            // 移除已处理的字节
            ctx->recv_buffer.erase(ctx->recv_buffer.begin(),
                                   ctx->recv_buffer.begin() + consumed);

            switch (frame->opcode) {
                case OpCode::kText:
                case OpCode::kBinary: {
                    // 特殊处理：文本 "__PING__" 响应 Pong
                    std::string payload_str(frame->payload.begin(), frame->payload.end());
                    if (payload_str == "__PING__") {
                        auto pong = FrameBuilder::BuildFrame(OpCode::kText,
                            std::vector<uint8_t>({'P', 'O', 'N', 'G'}));
                        g_server->SendData(connection_id, pong.data(), pong.size());
                        std::cout << "[PING] -> [PONG]" << std::endl;
                    } else {
                        auto response = FrameBuilder::BuildFrame(
                            frame->opcode, frame->payload);
                        g_server->SendData(connection_id, response.data(), response.size());
                        std::cout << "[ECHO] " << frame->payload.size() << " bytes" << std::endl;
                    }
                    break;
                }
                case OpCode::kPing: {
                    auto pong = FrameBuilder::BuildFrame(OpCode::kPong, frame->payload);
                    g_server->SendData(connection_id, pong.data(), pong.size());
                    break;
                }
                case OpCode::kPong:
                    break;
                case OpCode::kClose: {
                    auto close = FrameBuilder::CreateCloseFrame(1000, "");
                    g_server->SendData(connection_id, close.data(), close.size());
                    break;
                }
                default:
                    break;
            }
        }
    });

    // 断开连接
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
    std::cout << "Press Ctrl+C to stop." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down..." << std::endl;
    server.Stop();

    return 0;
}
