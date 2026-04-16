// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/jsonrpc/jsonrpc_server.h"

#include <algorithm>
#include <iostream>

namespace darwincore {
namespace websocket {

JsonRpcServer::JsonRpcServer()
    : rpc_handler_(std::make_unique<::darwincore::jsonrpc::RequestHandler>()) {}

JsonRpcServer::~JsonRpcServer() { Stop(); }

bool JsonRpcServer::Start(const std::string& /*host*/, uint16_t /*port*/) {
  // TODO: 集成 DarwinCore EventLoopGroup + Server
  // 目前预留接口，实际网络层实现待集成
  is_running_ = true;
  return true;
}

void JsonRpcServer::Stop() {
  if (!is_running_) return;

  is_running_ = false;

  // 清理连接
  std::vector<uint64_t> connection_ids;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (const auto& [id, _] : connections_) {
      connection_ids.push_back(id);
    }
    connections_.clear();
  }

  // 发送关闭帧
  for (uint64_t connection_id : connection_ids) {
    auto close_frame = FrameBuilder::CreateCloseFrame(1000, "Server shutting down");
    // network_server_->SendData(connection_id, ...);
  }
}

bool JsonRpcServer::IsRunning() const { return is_running_; }

void JsonRpcServer::RegisterMethod(const std::string& method,
                                   ::darwincore::jsonrpc::MethodHandler handler) {
  rpc_handler_->RegisterMethod(method, std::move(handler));
}

bool JsonRpcServer::SendNotification(uint64_t connection_id,
                                     const std::string& method,
                                     const nlohmann::json& params) {
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    if (connections_.find(connection_id) == connections_.end()) {
      return false;
    }
  }

  auto notification = jsonrpc::NotificationBuilder::Create(method, params);
  std::vector<uint8_t> payload(notification.begin(), notification.end());

  return SendWebSocketFrame(connection_id, payload, OpCode::kText);
}

void JsonRpcServer::BroadcastNotification(const std::string& method,
                                          const nlohmann::json& params) {
  auto notification = ::darwincore::jsonrpc::NotificationBuilder::Create(method, params);
  std::vector<uint8_t> payload(notification.begin(), notification.end());

  std::vector<uint64_t> connection_ids;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (const auto& [id, state] : connections_) {
      if (state->phase == ConnectionPhase::kWebSocket) {
        connection_ids.push_back(id);
      }
    }
  }

  for (uint64_t connection_id : connection_ids) {
    SendWebSocketFrame(connection_id, payload, OpCode::kText);
  }
}

size_t JsonRpcServer::GetConnectionCount() const {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  return connections_.size();
}

bool JsonRpcServer::CloseConnection(uint64_t connection_id, uint16_t code,
                                    const std::string& reason) {
  return CloseConnectionInternal(connection_id, code, reason, true);
}

bool JsonRpcServer::CloseConnectionInternal(uint64_t connection_id,
                                            uint16_t code,
                                            const std::string& reason,
                                            bool remove_from_map) {
  bool connection_exists = false;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connection_exists = connections_.find(connection_id) != connections_.end();
    if (connection_exists && remove_from_map) {
      connections_.erase(connection_id);
    }
  }

  auto close_frame = FrameBuilder::CreateCloseFrame(code, reason);
  // network_server_->SendData(connection_id, ...);

  return connection_exists;
}

void JsonRpcServer::SetOnClientConnected(
    std::function<void(uint64_t, const ConnectionInformation&)> callback) {
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

void JsonRpcServer::OnNetworkConnected(const ConnectionInformation& info) {
  auto state = std::make_shared<ConnectionState>();
  state->phase = ConnectionPhase::kHandshake;

  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_[info.connection_id] = state;
  }

  if (on_connected_) {
    on_connected_(info.connection_id, info);
  }
}

void JsonRpcServer::OnNetworkMessage(uint64_t connection_id,
                                     const std::vector<uint8_t>& data) {
  std::shared_ptr<ConnectionState> state_ptr;
  ConnectionPhase current_phase = ConnectionPhase::kHandshake;
  bool should_close = false;

  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    if (it == connections_.end()) return;

    state_ptr = it->second;
    current_phase = state_ptr->phase;

    size_t current_buffer_size = state_ptr->recv_buffer.size();
    size_t new_buffer_size = current_buffer_size + data.size();

    bool size_valid;
    if (current_phase == ConnectionPhase::kHandshake) {
      size_valid = IsHandshakeSizeValid(new_buffer_size);
    } else {
      size_valid = (new_buffer_size <= kMaxWebSocketFrameSize);
    }

    if (!size_valid) {
      connections_.erase(connection_id);
      should_close = true;
    } else {
      state_ptr->recv_buffer.insert(state_ptr->recv_buffer.end(),
                                    data.begin(), data.end());
    }
  }

  if (should_close) {
    CloseConnectionInternal(connection_id, 1009, "Message too big", false);
    return;
  }

  if (current_phase == ConnectionPhase::kHandshake) {
    // Process handshake...
  } else {
    // Process WebSocket frame...
  }
}

void JsonRpcServer::OnNetworkDisconnected(uint64_t connection_id) {
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(connection_id);
  }

  if (on_disconnected_) {
    on_disconnected_(connection_id);
  }
}

void JsonRpcServer::OnNetworkError(uint64_t connection_id,
                                   const std::string& message) {
  if (on_error_) {
    on_error_(connection_id, message);
  }
}

bool JsonRpcServer::HandleHandshake(uint64_t connection_id,
                                    const std::string& data) {
  HandshakeHandler handler;

  if (!handler.ParseRequest(data)) return false;

  std::string response = handler.GenerateResponse();
  if (response.empty()) return false;

  std::vector<uint8_t> response_data(response.begin(), response.end());
  // network_server_->SendData(connection_id, response_data.data(), response_data.size());

  return true;
}

void JsonRpcServer::HandleWebSocketFrame(uint64_t connection_id,
                                         const Frame& frame) {
  switch (frame.opcode) {
    case OpCode::kText: {
      std::string request(frame.payload.begin(), frame.payload.end());
      HandleJsonRpcRequest(connection_id, request);
      break;
    }
    case OpCode::kBinary: {
      SendWebSocketFrame(connection_id, frame.payload, OpCode::kBinary);
      break;
    }
    case OpCode::kPing: {
      SendWebSocketFrame(connection_id, frame.payload, OpCode::kPong);
      break;
    }
    case OpCode::kPong: {
      break;
    }
    case OpCode::kClose: {
      CloseConnection(connection_id, 1000, "Connection closed");
      break;
    }
    default:
      break;
  }
}

bool JsonRpcServer::SendWebSocketFrame(uint64_t connection_id,
                                       const std::vector<uint8_t>& payload,
                                       OpCode opcode) {
  auto frame = FrameBuilder::BuildFrame(opcode, payload);
  // return network_server_->SendData(connection_id, frame.data(), frame.size());
  return true;
}

void JsonRpcServer::HandleJsonRpcRequest(uint64_t connection_id,
                                         const std::string& request) {
  if (!is_running_) return;

  std::string response = rpc_handler_->HandleRequest(request);

  if (response.empty()) return;

  std::vector<uint8_t> response_data(response.begin(), response.end());
  SendWebSocketFrame(connection_id, response_data, OpCode::kText);
}

size_t JsonRpcServer::FindHttpRequestEnd(const std::vector<uint8_t>& buffer) {
  if (buffer.size() < 4) return std::string::npos;

  for (size_t i = 0; i <= buffer.size() - 4; ++i) {
    if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
        buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
      return static_cast<size_t>(i);
    }
  }
  return std::string::npos;
}

bool JsonRpcServer::IsHandshakeSizeValid(size_t size) {
  return size <= kMaxHandshakeSize;
}

}  // namespace websocket
}  // namespace darwincore
