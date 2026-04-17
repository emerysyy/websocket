// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/jsonrpc/jsonrpc_server.h"

#include <algorithm>
#include <iostream>

namespace darwincore {
namespace websocket {

JsonRpcServer::JsonRpcServer()
    : rpc_handler_(std::make_unique<::darwincore::jsonrpc::RequestHandler>()),
      network_server_(std::make_unique<darwincore::network::Server>()) {
  // 设置 DarwinCore 回调
  network_server_->SetOnClientConnected(
      [this](const darwincore::network::ConnectionInformation& info) {
        OnNetworkConnected(info);
      });

  network_server_->SetOnMessage(
      [this](uint64_t connection_id, const std::vector<uint8_t>& data) {
        OnNetworkMessage(connection_id, data);
      });

  network_server_->SetOnClientDisconnected(
      [this](uint64_t connection_id) {
        OnNetworkDisconnected(connection_id);
      });

  network_server_->SetOnConnectionError(
      [this](uint64_t connection_id,
             darwincore::network::NetworkError error,
             const std::string& message) {
        OnNetworkError(connection_id, message);
      });
}

JsonRpcServer::~JsonRpcServer() { Stop(); }

bool JsonRpcServer::Start(const std::string& host, uint16_t port) {
  if (is_running_) return false;

  if (!network_server_->StartIPv4(host, port)) {
    return false;
  }

  is_running_ = true;
  return true;
}

void JsonRpcServer::Stop() {
  if (!is_running_) return;

  is_running_ = false;
  network_server_->Stop();

  // 清理连接映射
  std::vector<ConnectionPtr> connections_to_close;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [id, conn] : connections_) {
      if (conn->IsConnected()) {
        connections_to_close.push_back(conn);
      }
    }
    connections_.clear();
  }

  // 发送关闭帧并清理
  for (const auto& conn : connections_to_close) {
    auto close_frame = FrameBuilder::CreateCloseFrame(1000, "Server shutting down");
    network_server_->SendData(conn->connection_id(),
                             close_frame.data(),
                             close_frame.size());
    conn->set_connected(false);
  }
}

bool JsonRpcServer::IsRunning() const { return is_running_; }

void JsonRpcServer::RegisterMethod(const std::string& method,
                                   ::darwincore::jsonrpc::MethodHandler handler) {
  rpc_handler_->RegisterMethod(method, std::move(handler));
}

bool JsonRpcServer::SendNotification(const ConnectionPtr& conn,
                                    const std::string& method,
                                    const nlohmann::json& params) {
  if (!conn || !conn->IsConnected()) {
    return false;
  }

  auto notification = jsonrpc::NotificationBuilder::Create(method, params);
  std::vector<uint8_t> payload(notification.begin(), notification.end());

  return SendWebSocketFrame(conn, payload, OpCode::kText);
}

void JsonRpcServer::BroadcastNotification(const std::string& method,
                                         const nlohmann::json& params) {
  auto notification = ::darwincore::jsonrpc::NotificationBuilder::Create(method, params);
  std::vector<uint8_t> payload(notification.begin(), notification.end());

  std::vector<ConnectionPtr> connections;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [id, conn] : connections_) {
      // 只对已建立 WebSocket 连接的客户端广播
      if (conn->phase() == SessionPhase::kWebSocket && conn->IsConnected()) {
        connections.push_back(conn);
      }
    }
  }

  for (const auto& conn : connections) {
    SendWebSocketFrame(conn, payload, OpCode::kText);
  }
}

size_t JsonRpcServer::GetConnectionCount() const {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  return connections_.size();
}

bool JsonRpcServer::CloseConnection(const ConnectionPtr& conn, uint16_t code,
                                    const std::string& reason) {
  CloseConnectionInternal(conn, code, reason);
  return true;
}

void JsonRpcServer::CloseConnectionInternal(const ConnectionPtr& conn,
                                            uint16_t code,
                                            const std::string& reason) {
  if (!conn) return;
  if (conn->is_closing()) return;  // 幂等性保护

  conn->set_closing(true);
  conn->set_phase(SessionPhase::kClosing);

  auto close_frame = FrameBuilder::CreateCloseFrame(code, reason);
  network_server_->SendData(conn->connection_id(),
                           close_frame.data(),
                           close_frame.size());

  // 从连接映射中移除
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(conn->connection_id());
  }

  conn->set_connected(false);
}

void JsonRpcServer::SetOnClientConnected(
    std::function<void(const ConnectionPtr&)> callback) {
  on_connected_ = std::move(callback);
}

void JsonRpcServer::SetOnClientDisconnected(
    std::function<void(const ConnectionPtr&)> callback) {
  on_disconnected_ = std::move(callback);
}

void JsonRpcServer::SetOnError(
    std::function<void(const ConnectionPtr&, const std::string&)> callback) {
  on_error_ = std::move(callback);
}

ConnectionPtr JsonRpcServer::GetConnection(uint64_t connection_id) {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  auto it = connections_.find(connection_id);
  if (it != connections_.end()) {
    return it->second;
  }
  return nullptr;
}

void JsonRpcServer::OnNetworkConnected(
    const darwincore::network::ConnectionInformation& info) {
  // 创建连接包装器（继承 WebSocketSession，会话状态在 Connection 自身）
  auto conn = std::make_shared<Connection>(info.connection_id, info.peer_address);

  // 加入连接映射（仅用于 id → ConnectionPtr 的反向查找）
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_[info.connection_id] = conn;
  }

  // 触发用户回调
  if (on_connected_) {
    on_connected_(conn);
  }
}

void JsonRpcServer::OnNetworkMessage(uint64_t connection_id,
                                     const std::vector<uint8_t>& data) {
  auto conn = GetConnection(connection_id);
  if (!conn) return;

  // 追加数据到接收缓冲区
  size_t current_size = conn->recv_buffer().size();
  size_t new_size = current_size + data.size();

  bool size_valid;
  if (conn->phase() == SessionPhase::kHandshake) {
    size_valid = IsHandshakeSizeValid(new_size);
  } else {
    size_valid = (new_size <= kMaxWebSocketFrameSize);
  }

  if (!size_valid) {
    CloseConnectionInternal(conn, 1009, "Message too big");
    return;
  }

  conn->recv_buffer().insert(conn->recv_buffer().end(), data.begin(), data.end());

  // 根据阶段处理数据
  if (conn->phase() == SessionPhase::kHandshake) {
    // 处理 HTTP 握手请求
    size_t header_end = FindHttpRequestEnd(conn->recv_buffer());
    if (header_end != std::string::npos) {
      std::string request(reinterpret_cast<const char*>(conn->recv_buffer().data()),
                         header_end + 4);
      HandleHandshake(conn, request);
      // 移除已处理的握手数据
      conn->recv_buffer().erase(conn->recv_buffer().begin(),
                                conn->recv_buffer().begin() + header_end + 4);
    }
  } else if (conn->phase() == SessionPhase::kWebSocket) {
    // 处理 WebSocket 帧
    ProcessWebSocketFrames(conn);
  }
}

void JsonRpcServer::ProcessWebSocketFrames(const ConnectionPtr& conn) {
  if (!conn || conn->recv_buffer().empty()) return;

  FrameParser* parser = conn->parser();
  if (!parser) return;

  // 循环解析所有完整的帧
  while (!conn->recv_buffer().empty()) {
    size_t consumed = 0;
    auto frame_opt = parser->Parse(conn->recv_buffer(), consumed);

    if (!frame_opt) {
      // 数据不完整，等待更多数据
      break;
    }

    // 移除已解析的帧数据
    conn->consume_recv_buffer(consumed);

    // 处理帧
    HandleWebSocketFrame(conn, *frame_opt);

    // 如果连接已关闭，停止处理
    if (!conn->IsConnected() || conn->phase() == SessionPhase::kClosed) {
      break;
    }
  }
}

void JsonRpcServer::OnNetworkDisconnected(uint64_t connection_id) {
  ConnectionPtr conn;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
      conn = it->second;
      connections_.erase(it);
    }
  }

  if (conn) {
    conn->set_connected(false);
    conn->set_phase(SessionPhase::kClosed);
    if (on_disconnected_) {
      on_disconnected_(conn);
    }
  }
}

void JsonRpcServer::OnNetworkError(uint64_t connection_id,
                                  const std::string& message) {
  auto conn = GetConnection(connection_id);
  if (conn && on_error_) {
    on_error_(conn, message);
  }
}

void JsonRpcServer::HandleHandshake(const ConnectionPtr& conn,
                                    const std::string& data) {
  HandshakeHandler handler;

  if (!handler.ParseRequest(data)) return;

  std::string response = handler.GenerateResponse();
  if (response.empty()) return;

  // 发送握手响应
  network_server_->SendData(conn->connection_id(),
                           reinterpret_cast<const uint8_t*>(response.data()),
                           response.size());

  // 更新会话阶段
  conn->set_phase(SessionPhase::kWebSocket);
}

void JsonRpcServer::HandleWebSocketFrame(const ConnectionPtr& conn,
                                         const Frame& frame) {
  switch (frame.opcode) {
    case OpCode::kText: {
      std::string request(frame.payload.begin(), frame.payload.end());
      HandleJsonRpcRequest(conn, request);
      break;
    }
    case OpCode::kBinary: {
      SendWebSocketFrame(conn, frame.payload, OpCode::kBinary);
      break;
    }
    case OpCode::kPing: {
      SendWebSocketFrame(conn, frame.payload, OpCode::kPong);
      break;
    }
    case OpCode::kPong: {
      // Pong 帧不需要处理
      break;
    }
    case OpCode::kClose: {
      // 收到 Close 帧，优雅关闭
      CloseConnection(conn, 1000, "Connection closed by client");
      break;
    }
    case OpCode::kContinuation: {
      // 分片帧的中间或结束帧
      // 简化处理：直接追加到上次消息
      break;
    }
    default:
      break;
  }
}

bool JsonRpcServer::SendWebSocketFrame(const ConnectionPtr& conn,
                                      const std::vector<uint8_t>& payload,
                                      OpCode opcode) {
  if (!conn || !conn->IsConnected()) return false;

  auto frame = FrameBuilder::BuildFrame(opcode, payload);
  return network_server_->SendData(conn->connection_id(),
                                   frame.data(),
                                   frame.size());
}

void JsonRpcServer::HandleJsonRpcRequest(const ConnectionPtr& conn,
                                        const std::string& request) {
  if (!is_running_ || !conn) return;

  std::string response = rpc_handler_->HandleRequest(request);

  if (response.empty()) return;

  std::vector<uint8_t> response_data(response.begin(), response.end());
  SendWebSocketFrame(conn, response_data, OpCode::kText);
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
