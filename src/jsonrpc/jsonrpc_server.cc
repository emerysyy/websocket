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
      auto* ctx = GetContext(conn);
      if (ctx && ctx->phase == ConnectionPhase::kWebSocket) {
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

ConnectionContext* JsonRpcServer::GetOrCreateContext(const ConnectionPtr& conn) {
  if (!conn) return nullptr;

  auto* ctx = conn->GetSessionState();
  if (ctx) {
    return ctx;
  }

  // 创建新上下文并绑定到 Connection
  ConnectionContext new_ctx;
  new_ctx.phase = ConnectionPhase::kHandshake;
  new_ctx.connection_id = conn->connection_id();
  conn->SetContext(std::make_any<ConnectionContext>(new_ctx));

  return conn->GetSessionState();
}

ConnectionContext* JsonRpcServer::GetContext(const ConnectionPtr& conn) {
  if (!conn) return nullptr;
  return conn->GetSessionState();
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
  // 创建连接包装器
  auto conn = std::make_shared<Connection>(info.connection_id);

  // 创建会话状态
  ConnectionContext ctx;
  ctx.phase = ConnectionPhase::kHandshake;
  ctx.connection_id = info.connection_id;
  ctx.remote_address = info.peer_address;
  conn->SetContext(std::make_any<ConnectionContext>(ctx));

  // 加入连接映射
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

  auto* ctx = GetOrCreateContext(conn);
  if (!ctx) return;

  // 追加数据到接收缓冲区
  size_t current_size = ctx->recv_buffer.size();
  size_t new_size = current_size + data.size();

  bool size_valid;
  if (ctx->phase == ConnectionPhase::kHandshake) {
    size_valid = IsHandshakeSizeValid(new_size);
  } else {
    size_valid = (new_size <= kMaxWebSocketFrameSize);
  }

  if (!size_valid) {
    CloseConnectionInternal(conn, 1009, "Message too big");
    return;
  }

  ctx->recv_buffer.insert(ctx->recv_buffer.end(), data.begin(), data.end());

  // 处理数据
  if (ctx->phase == ConnectionPhase::kHandshake) {
    size_t header_end = FindHttpRequestEnd(ctx->recv_buffer);
    if (header_end != std::string::npos) {
      std::string request(reinterpret_cast<const char*>(ctx->recv_buffer.data()),
                         header_end + 4);
      HandleHandshake(conn, request);
      // 移除已处理的握手数据
      ctx->recv_buffer.erase(ctx->recv_buffer.begin(),
                            ctx->recv_buffer.begin() + header_end + 4);
    }
  } else {
    // 处理 WebSocket 帧 - 使用 FrameParser
    // 注意：简化处理，实际需要完整解析
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

  // 更新会话阶段（直接操作 Connection::context_ 中的 ConnectionContext）
  auto* ctx = GetContext(conn);
  if (ctx) {
    ctx->phase = ConnectionPhase::kWebSocket;
  }
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
      break;
    }
    case OpCode::kClose: {
      CloseConnection(conn, 1000, "Connection closed");
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
