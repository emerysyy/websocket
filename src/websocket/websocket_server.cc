// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/websocket_server.h"

#include <iostream>

namespace darwincore {
namespace websocket {

WebSocketServer::WebSocketServer() : network_server_(nullptr) {}

WebSocketServer::~WebSocketServer() { Stop(); }

bool WebSocketServer::Start(const std::string& host, uint16_t port) {
  if (is_running_.load()) {
    std::cerr << "[WebSocketServer] Already running" << std::endl;
    return false;
  }

  network_server_ = std::make_unique<darwincore::network::Server>();

  // 设置回调
  network_server_->SetOnClientConnected([this](
      const darwincore::network::ConnectionInformation& info) {
    OnNetworkConnected(info);
  });

  network_server_->SetOnMessage([this](uint64_t conn_id,
                                       const std::vector<uint8_t>& data) {
    OnNetworkMessage(conn_id, data);
  });

  network_server_->SetOnClientDisconnected(
      [this](uint64_t conn_id) { OnNetworkDisconnected(conn_id); });

  if (!network_server_->StartIPv4(host, port)) {
    std::cerr << "[WebSocketServer] Failed to start server on " << host
              << ":" << port << std::endl;
    network_server_.reset();
    return false;
  }

  is_running_.store(true);
  std::cout << "[WebSocketServer] Listening on ws://" << host << ":"
            << port << std::endl;
  return true;
}

void WebSocketServer::Stop() {
  if (!is_running_.load()) {
    return;
  }

  is_running_.store(false);

  if (network_server_) {
    network_server_->Stop();
    network_server_.reset();
  }

  std::lock_guard<std::mutex> lock(connections_mutex_);
  connections_.clear();
}

bool WebSocketServer::IsRunning() const { return is_running_.load(); }

size_t WebSocketServer::GetConnectionCount() const {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  return connections_.size();
}

bool WebSocketServer::SendFrame(const ConnectionPtr& conn,
                                const std::vector<uint8_t>& payload,
                                OpCode opcode) {
  if (!conn || !conn->IsConnected()) {
    return false;
  }

  auto frame = FrameBuilder::BuildFrame(opcode, payload);
  network_server_->SendData(conn->connection_id(), frame.data(),
                             frame.size());
  return true;
}

bool WebSocketServer::SendText(const ConnectionPtr& conn,
                               const std::string& text) {
  std::vector<uint8_t> payload(text.begin(), text.end());
  return SendFrame(conn, payload, OpCode::kText);
}

bool WebSocketServer::SendBinary(const ConnectionPtr& conn,
                                 const std::vector<uint8_t>& data) {
  return SendFrame(conn, data, OpCode::kBinary);
}

bool WebSocketServer::SendPing(const ConnectionPtr& conn,
                               const std::vector<uint8_t>& payload) {
  if (payload.size() > 125) {
    std::cerr << "[WebSocketServer] Ping payload too large (max 125 bytes)"
              << std::endl;
    return false;
  }
  return SendFrame(conn, payload, OpCode::kPing);
}

bool WebSocketServer::SendPong(const ConnectionPtr& conn,
                               const std::vector<uint8_t>& payload) {
  if (payload.size() > 125) {
    std::cerr << "[WebSocketServer] Pong payload too large (max 125 bytes)"
              << std::endl;
    return false;
  }
  return SendFrame(conn, payload, OpCode::kPong);
}

bool WebSocketServer::Close(const ConnectionPtr& conn, uint16_t code,
                             const std::string& reason) {
  if (!conn || !conn->IsConnected()) {
    return false;
  }

  auto close_frame = FrameBuilder::CreateCloseFrame(code, reason);
  network_server_->SendData(conn->connection_id(), close_frame.data(),
                             close_frame.size());

  conn->set_closing(true);
  return true;
}

void WebSocketServer::ForceClose(const ConnectionPtr& conn) {
  if (!conn) {
    return;
  }

  auto connection_id = conn->connection_id();

  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(connection_id);
  }

  conn->set_connected(false);
  conn->set_phase(SessionPhase::kClosed);

  if (on_disconnected_) {
    on_disconnected_(conn);
  }
}

size_t WebSocketServer::Broadcast(const std::vector<uint8_t>& payload,
                                   OpCode opcode) {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  size_t count = 0;
  for (const auto& [id, conn] : connections_) {
    // 只广播已升级到 WebSocket 阶段的连接（过滤握手阶段）
    if (conn->IsConnected() && !conn->is_closing() &&
        conn->phase() == SessionPhase::kWebSocket) {
      auto frame = FrameBuilder::BuildFrame(opcode, payload);
      network_server_->SendData(id, frame.data(), frame.size());
      ++count;
    }
  }
  return count;
}

void WebSocketServer::SetOnFrame(FrameCallback callback) {
  on_frame_ = std::move(callback);
}

void WebSocketServer::SetOnConnected(
    std::function<void(const ConnectionPtr&)> callback) {
  on_connected_ = std::move(callback);
}

void WebSocketServer::SetOnDisconnected(
    std::function<void(const ConnectionPtr&)> callback) {
  on_disconnected_ = std::move(callback);
}

void WebSocketServer::SetOnError(
    std::function<void(const ConnectionPtr&, const std::string&)> callback) {
  on_error_ = std::move(callback);
}

ConnectionPtr WebSocketServer::GetConnection(uint64_t connection_id) {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  auto it = connections_.find(connection_id);
  if (it != connections_.end()) {
    return it->second;
  }
  return nullptr;
}

size_t WebSocketServer::FindHttpRequestEnd(
    const std::vector<uint8_t>& buffer) {
  if (buffer.size() < 4) {
    return 0;
  }
  for (size_t i = 0; i + 3 < buffer.size(); ++i) {
    if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
        buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
      return i + 4;
    }
  }
  return 0;
}

void WebSocketServer::OnNetworkConnected(
    const darwincore::network::ConnectionInformation& info) {
  auto conn =
      std::make_shared<Connection>(info.connection_id, info.peer_address);

  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_[info.connection_id] = conn;
  }

  std::cout << "[WebSocketServer] Client connected: " << info.connection_id
            << " from " << info.peer_address << std::endl;

  if (on_connected_) {
    on_connected_(conn);
  }
}

void WebSocketServer::OnNetworkMessage(uint64_t connection_id,
                                       const std::vector<uint8_t>& data) {
  auto conn = GetConnection(connection_id);
  if (!conn) {
    return;
  }

  // 追加数据到缓冲区
  conn->recv_buffer().insert(conn->recv_buffer().end(), data.begin(),
                               data.end());

  // 清理缓冲区阈值检查
  if (conn->recv_buffer().size() > kMaxHandshakeSize * 2) {
    if (conn->phase() == SessionPhase::kHandshake) {
      std::cerr
          << "[WebSocketServer] Handshake buffer too large, rejecting"
          << std::endl;
      if (on_error_) {
        on_error_(conn, "Handshake buffer overflow");
      }
      const uint8_t kBadRequest[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
      network_server_->SendData(connection_id, kBadRequest,
                                sizeof(kBadRequest) - 1);
      return;
    }
  }

  // 根据阶段处理
  if (conn->phase() == SessionPhase::kHandshake) {
    size_t http_end = FindHttpRequestEnd(conn->recv_buffer());
    if (http_end == 0) {
      // HTTP 请求不完整，等待更多数据
      return;
    }

    std::string http_request(conn->recv_buffer().begin(),
                             conn->recv_buffer().begin() + http_end);
    HandleHandshake(conn, http_request);
  } else if (conn->phase() == SessionPhase::kWebSocket) {
    ProcessWebSocketFrames(conn);
  }
  // kClosing 和 kClosed 阶段不处理数据
}

void WebSocketServer::OnNetworkDisconnected(uint64_t connection_id) {
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
    std::cout << "[WebSocketServer] Client disconnected: " << connection_id
              << std::endl;

    if (on_disconnected_) {
      on_disconnected_(conn);
    }
  }
}

void WebSocketServer::HandleHandshake(const ConnectionPtr& conn,
                                       const std::string& data) {
  if (conn->recv_buffer().size() > kMaxHandshakeSize) {
    std::cerr << "[WebSocketServer] Handshake too large" << std::endl;
    if (on_error_) {
      on_error_(conn, "Handshake too large");
    }
    const uint8_t kBadRequest[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
    network_server_->SendData(conn->connection_id(), kBadRequest,
                              sizeof(kBadRequest) - 1);
    return;
  }

  HandshakeHandler handler;
  if (!handler.ParseRequest(data)) {
    std::cerr << "[WebSocketServer] Invalid handshake" << std::endl;
    if (on_error_) {
      on_error_(conn, "Invalid handshake");
    }
    auto error_resp = handler.GenerateErrorResponse(handler.GetLastError());
    network_server_->SendData(conn->connection_id(),
                              reinterpret_cast<const uint8_t*>(error_resp.data()),
                              error_resp.size());
    return;
  }

  std::string response = handler.GenerateResponse();
  network_server_->SendData(conn->connection_id(),
                            reinterpret_cast<const uint8_t*>(response.data()),
                            response.size());

  // 移除已处理的 HTTP 数据
  size_t http_end = FindHttpRequestEnd(conn->recv_buffer());
  conn->ConsumeRecvBuffer(http_end);

  // 切换到 WebSocket 阶段
  conn->set_phase(SessionPhase::kWebSocket);

  std::cout << "[WebSocketServer] Handshake completed: " << conn->connection_id()
            << std::endl;

  // 继续处理握手后紧跟的 WebSocket 数据（TCP 可能合并发送）
  if (!conn->recv_buffer().empty()) {
    ProcessWebSocketFrames(conn);
  }
}

void WebSocketServer::ProcessWebSocketFrames(const ConnectionPtr& conn) {
  if (conn->phase() != SessionPhase::kWebSocket) {
    return;
  }

  // 关闭态不处理新帧
  if (conn->is_closing() || conn->phase() == SessionPhase::kClosing ||
      conn->phase() == SessionPhase::kClosed) {
    return;
  }

  while (true) {
    if (conn->recv_buffer().empty()) {
      break;
    }

    size_t consumed = 0;
    auto frame = conn->parser()->Parse(conn->recv_buffer(), consumed);

    if (!frame) {
      // 数据不完整，等待更多数据
      break;
    }

    // 移除已处理的字节
    conn->ConsumeRecvBuffer(consumed);

    // 协议约束校验
    try {
      conn->parser()->ValidateControlFrameConstraints(*frame);
    } catch (const ParseError& e) {
      std::cerr << "[WebSocketServer] Protocol error: " << static_cast<int>(e)
                << std::endl;
      if (on_error_) {
        on_error_(conn, "Protocol error: invalid control frame");
      }
      // 发送关闭帧并关闭
      auto close_frame = FrameBuilder::CreateCloseFrame(1002, "Protocol error");
      network_server_->SendData(conn->connection_id(), close_frame.data(),
                                 close_frame.size());
      conn->set_phase(SessionPhase::kClosed);
      return;
    }

    // 处理帧
    HandleFrame(conn, *frame);

    // 检查缓冲区是否为空
    if (conn->recv_buffer().empty()) {
      break;
    }
  }
}

void WebSocketServer::HandleFrame(const ConnectionPtr& conn,
                                  const Frame& frame) {
  switch (frame.opcode) {
    case OpCode::kText:
    case OpCode::kBinary:
    case OpCode::kContinuation:
      if (on_frame_) {
        on_frame_(conn, frame);
      }
      break;

    case OpCode::kPing:
      // 自动回复 Pong
      SendPong(conn, frame.payload);
      break;

    case OpCode::kPong:
      // 被动 Pong，无需处理
      break;

    case OpCode::kClose:
      // 回应关闭帧
      if (!conn->is_closing()) {
        conn->set_closing(true);
        auto close_frame = FrameBuilder::CreateCloseFrame(1000, "");
        network_server_->SendData(conn->connection_id(), close_frame.data(),
                                 close_frame.size());
        conn->set_phase(SessionPhase::kClosed);
      }
      break;

    default:
      std::cerr << "[WebSocketServer] Unknown opcode: "
                << static_cast<int>(frame.opcode) << std::endl;
      break;
  }
}

}  // namespace websocket
}  // namespace darwincore
