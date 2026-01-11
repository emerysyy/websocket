// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/jsonrpc_server.h"
#include "darwincore/websocket/jsonrpc/notification_builder.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <darwincore/network/server.h>
#include <darwincore/websocket/frame_parser.h>

namespace darwincore {
namespace websocket {

JsonRpcServer::JsonRpcServer()
    : rpc_handler_(std::make_unique<jsonrpc::RequestHandler>()) {
}

JsonRpcServer::~JsonRpcServer() {
    Stop();
}

bool JsonRpcServer::Start(const std::string& host, uint16_t port) {
    if (is_running_) {
        return true;
    }

    try {
        // 创建 Server 对象（必须在堆上）
        network_server_ = std::make_unique<network::Server>();

        // 使用原始指针捕获（避免 this 指针问题）
        auto* server_ptr = this;

        // 设置事件回调（必须在 StartIPv4 之前！）
        network_server_->SetOnClientConnected([server_ptr](
            const network::ConnectionInformation& info) {
            server_ptr->OnNetworkConnected(info);
        });

        network_server_->SetOnMessage([server_ptr](
            uint64_t connection_id,
            const std::vector<uint8_t>& data) {
            std::cout << "[###] SetOnMessage callback triggered: conn_id=" << connection_id
                      << ", size=" << data.size() << std::endl;
            server_ptr->OnNetworkMessage(connection_id, data);
        });

        network_server_->SetOnClientDisconnected([server_ptr](uint64_t connection_id) {
            server_ptr->OnNetworkDisconnected(connection_id);
        });

        network_server_->SetOnConnectionError([server_ptr](
            uint64_t connection_id,
            network::NetworkError error,
            const std::string& message) {
            server_ptr->OnNetworkError(connection_id, error, message);
        });

        // 启动服务器
        if (!network_server_->StartIPv4(host, port)) {
            return false;
        }

        is_running_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in Start: " << e.what() << std::endl;
        return false;
    }
}

void JsonRpcServer::Stop() {
    if (!is_running_) {
        return;
    }

    // 关闭所有连接
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (const auto& [connection_id, state] : connections_) {
            auto close_frame = FrameBuilder::CreateCloseFrame(1000, "Server shutting down");
            network_server_->SendData(connection_id, close_frame.data(), close_frame.size());
        }
        connections_.clear();
    }

    // 停止网络服务器
    network_server_->Stop();

    is_running_ = false;
}

bool JsonRpcServer::IsRunning() const {
    return is_running_;
}

void JsonRpcServer::RegisterMethod(const std::string& method,
                                    jsonrpc::MethodHandler handler) {
    rpc_handler_->RegisterMethod(method, std::move(handler));
}

bool JsonRpcServer::SendNotification(uint64_t connection_id,
                                      const std::string& method,
                                      const nlohmann::json& params) {
    auto notification = jsonrpc::NotificationBuilder::Create(method, params);
    std::vector<uint8_t> payload(notification.begin(), notification.end());
    
    return SendWebSocketFrame(connection_id, payload, OpCode::kText);
}

void JsonRpcServer::BroadcastNotification(const std::string& method,
                                           const nlohmann::json& params) {
    auto notification = jsonrpc::NotificationBuilder::Create(method, params);
    std::vector<uint8_t> payload(notification.begin(), notification.end());
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (const auto& [connection_id, state] : connections_) {
        if (state.handshake_completed) {
            SendWebSocketFrame(connection_id, payload, OpCode::kText);
        }
    }
}

size_t JsonRpcServer::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

void JsonRpcServer::CloseConnection(uint64_t connection_id,
                                     uint16_t code,
                                     const std::string& reason) {
    auto close_frame = FrameBuilder::CreateCloseFrame(code, reason);
    network_server_->SendData(connection_id, close_frame.data(), close_frame.size());
    
    // 移除连接状态
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(connection_id);
}

void JsonRpcServer::SetOnClientConnected(
    std::function<void(uint64_t, const network::ConnectionInformation&)> callback) {
    on_connected_ = std::move(callback);
}

void JsonRpcServer::SetOnClientDisconnected(
    std::function<void(uint64_t)> callback) {
    on_disconnected_ = std::move(callback);
}

void JsonRpcServer::SetOnError(
    std::function<void(uint64_t, const std::string&)> callback) {
    on_error_ = std::move(callback);
}

void JsonRpcServer::OnNetworkConnected(const network::ConnectionInformation& info) {
    // 创建新的连接状态
    ConnectionState state;
    state.parser = std::make_unique<FrameParser>();

    // 先添加连接
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[info.connection_id] = std::move(state);
    }

    // 在锁外调用用户回调，避免死锁（回调可能会调用 GetConnectionCount 等需要锁的方法）
    if (on_connected_) {
        on_connected_(info.connection_id, info);
    }
}

void JsonRpcServer::OnNetworkMessage(uint64_t connection_id,
                                      const std::vector<uint8_t>& data) {
    std::cout << "[***] OnNetworkMessage called: conn_id=" << connection_id
              << ", size=" << data.size() << std::endl;

    std::lock_guard<std::mutex> lock(connections_mutex_);

    auto it = connections_.find(connection_id);
    if (it == connections_.end()) {
        return;
    }

    ConnectionState& state = it->second;

    // 如果还没完成握手，尝试处理握手
    if (!state.handshake_completed) {
        std::string request(data.begin(), data.end());
        if (HandleHandshake(connection_id, request)) {
            state.handshake_completed = true;
        }
        return;
    }

    // 追加数据到缓冲区
    state.recv_buffer.insert(state.recv_buffer.end(), data.begin(), data.end());

    // 循环解析帧
    while (true) {
        size_t consumed = 0;
        auto frame = state.parser->Parse(state.recv_buffer, consumed);

        if (!frame.has_value()) {
            // 数据不完整，等待更多数据
            break;
        }

        // 移除已处理的数据
        state.recv_buffer.erase(state.recv_buffer.begin(),
                                state.recv_buffer.begin() + consumed);

        // 处理帧
        HandleWebSocketFrame(connection_id, *frame);
    }
}

void JsonRpcServer::OnNetworkDisconnected(uint64_t connection_id) {
    // 移除连接状态
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(connection_id);
    }

    // 调用用户回调
    if (on_disconnected_) {
        on_disconnected_(connection_id);
    }
}

void JsonRpcServer::OnNetworkError(uint64_t connection_id,
                                    network::NetworkError /*error*/,
                                    const std::string& message) {
    // 调用用户回调
    if (on_error_) {
        on_error_(connection_id, message);
    }
}

bool JsonRpcServer::HandleHandshake(uint64_t connection_id,
                                     const std::string& data) {
    HandshakeHandler handler;
    
    if (!handler.ParseRequest(data)) {
        return false;
    }
    
    std::string response = handler.GenerateResponse();
    if (response.empty()) {
        return false;
    }
    
    // 发送握手响应
    std::vector<uint8_t> response_data(response.begin(), response.end());
    network_server_->SendData(connection_id, response_data.data(), response_data.size());
    
    return true;
}

void JsonRpcServer::HandleWebSocketFrame(uint64_t connection_id,
                                          const Frame& frame) {
    switch (frame.opcode) {
        case OpCode::kText: {
            // 处理文本帧
            std::string request(frame.payload.begin(), frame.payload.end());
            HandleJsonRpcRequest(connection_id, request);
            break;
        }
        
        case OpCode::kBinary: {
            // 二进制帧不支持
            break;
        }
        
        case OpCode::kPing: {
            // 响应 Pong
            SendWebSocketFrame(connection_id, frame.payload, OpCode::kPong);
            break;
        }
        
        case OpCode::kPong: {
            // Pong 不需要处理
            break;
        }
        
        case OpCode::kClose: {
            // 发送 Close 响应并关闭连接
            CloseConnection(connection_id, 1000, "Connection closed");
            break;
        }
        
        case OpCode::kContinuation:
        default:
            // 不支持的分片
            break;
    }
}

bool JsonRpcServer::SendWebSocketFrame(uint64_t connection_id,
                                        const std::vector<uint8_t>& payload,
                                        OpCode opcode) {
    auto frame = FrameBuilder::BuildFrame(opcode, payload);
    return network_server_->SendData(connection_id, frame.data(), frame.size());
}

void JsonRpcServer::HandleJsonRpcRequest(uint64_t connection_id,
                                          const std::string& request) {
    std::string response = rpc_handler_->HandleRequest(request);
    
    // 通知不需要响应
    if (response.empty()) {
        return;
    }
    
    // 发送响应
    std::vector<uint8_t> response_data(response.begin(), response.end());
    SendWebSocketFrame(connection_id, response_data, OpCode::kText);
}

}  // namespace websocket
}  // namespace darwincore
