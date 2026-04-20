// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/websocket_server.h"

#include <iostream>

#include <darwincore/network/base/event_loop_group.h>

namespace darwincore {
namespace websocket {

WebSocketServer::WebSocketServer()
    : loop_group_(1, "ws-io"), network_server_(nullptr) {}

WebSocketServer::~WebSocketServer() { Stop(); }

bool WebSocketServer::Start(const std::string& host, uint16_t port) {
  if (is_running_.load()) {
    std::cerr << "[WebSocketServer] Already running" << std::endl;
    return false;
  }

  // 启动 I/O loop 线程
  if (!loop_group_.Start()) {
    std::cerr << "[WebSocketServer] Failed to start I/O loop group" << std::endl;
    return false;
  }

  network_server_ = std::make_unique<darwincore::network::Server>(loop_group_, "ws-server");

  // 设置连接回调（连接/断开共用，通过 conn->IsConnected() 区分）
  network_server_->SetConnectionCallback([this](const darwincore::network::ConnectionPtr& conn) {
    if (conn->IsConnected()) {
      OnNetworkConnected(conn);
    } else {
      OnNetworkDisconnected(conn);
    }
  });

  // 设置消息回调
  network_server_->SetMessageCallback([this](const darwincore::network::ConnectionPtr& conn,
                                             darwincore::network::Buffer& buffer,
                                             [[maybe_unused]] darwincore::network::Timestamp recv_time) {
    std::vector<uint8_t> data(buffer.Peek(), buffer.Peek() + buffer.ReadableBytes());
    OnNetworkMessage(conn, data);
    buffer.RetrieveAll();  // 清空 buffer
  });

  if (!network_server_->StartIPv4(host, port)) {
    std::cerr << "[WebSocketServer] Failed to start server on " << host
              << ":" << port << std::endl;
    network_server_.reset();
    loop_group_.Stop();
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

  loop_group_.Stop();

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

  // 正在关闭的连接不应再发送业务帧
  if (conn->is_closing()) {
    return false;
  }

  auto net_conn = conn->network_conn();
  if (!net_conn) {
    return false;
  }

  auto frame = FrameBuilder::BuildFrame(opcode, payload);
  net_conn->Send(frame.data(), frame.size());
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

  // 已经在关闭中，幂等返回
  if (conn->is_closing()) {
    return true;
  }

  conn->set_closing(true);

  // 发送 Close 帧（如果有网络连接）
  auto net_conn = conn->network_conn();
  if (net_conn) {
    auto close_frame = FrameBuilder::CreateCloseFrame(code, reason);
    net_conn->Send(close_frame.data(), close_frame.size());
  }

  return true;
}

void WebSocketServer::ForceClose(const ConnectionPtr& conn, uint16_t code,
                                 const std::string& reason) {
  if (!conn || !conn->IsConnected()) {
    return;  // 已经是断开状态，幂等
  }

  const bool was_closing = conn->is_closing();
  conn->set_closing(true);

  // 已经处于 Close 流程时，不再重复发送 Close 帧
  if (!was_closing) {
    auto net_conn = conn->network_conn();
    if (net_conn) {
      auto close_frame = FrameBuilder::CreateCloseFrame(code, reason);
      net_conn->Send(close_frame.data(), close_frame.size());
    }
  }

  uint64_t conn_id = conn->connection_id();
  auto net_conn = conn->network_conn();
  if (net_conn) {
    conn_id = net_conn->GetConnectionId();
  }

  // 清理本地状态
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(conn_id);
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
  auto frame = FrameBuilder::BuildFrame(opcode, payload);
  for (const auto& [id, conn] : connections_) {
    // 只广播已升级到 WebSocket 阶段的连接（过滤握手阶段）
    if (conn->IsConnected() && !conn->is_closing() &&
        conn->phase() == SessionPhase::kWebSocket) {
      ++count;  // 计入符合条件的连接
      // 实际发送需要网络连接（测试用连接可能没有）
      auto net_conn = conn->network_conn();
      if (net_conn) {
        net_conn->Send(frame.data(), frame.size());
      }
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
    const darwincore::network::ConnectionPtr& net_conn) {
  auto conn = std::make_shared<Connection>(net_conn->GetConnectionId(),
                                           net_conn->PeerAddr());
  conn->set_network_conn(net_conn);

  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_[net_conn->GetConnectionId()] = conn;
  }

  std::cout << "[WebSocketServer] Client connected: " << net_conn->GetConnectionId()
            << " from " << net_conn->PeerAddr() << std::endl;

  if (on_connected_) {
    on_connected_(conn);
  }
}

void WebSocketServer::OnNetworkMessage(const darwincore::network::ConnectionPtr& net_conn,
                                       const std::vector<uint8_t>& data) {
  if (!net_conn) {
    return;
  }

  ConnectionPtr conn;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(net_conn->GetConnectionId());
    if (it != connections_.end()) {
      conn = it->second;
    }
  }
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
      net_conn->Send(kBadRequest, sizeof(kBadRequest) - 1);
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

void WebSocketServer::OnNetworkDisconnected(
    const darwincore::network::ConnectionPtr& net_conn) {
  ConnectionPtr conn;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(net_conn->GetConnectionId());
    if (it != connections_.end()) {
      conn = it->second;
      connections_.erase(it);
    }
  }

  if (conn) {
    conn->set_connected(false);
    std::cout << "[WebSocketServer] Client disconnected: " << net_conn->GetConnectionId()
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
    conn->network_conn()->Send(kBadRequest, sizeof(kBadRequest) - 1);
    return;
  }

  HandshakeHandler handler;
  if (!handler.ParseRequest(data)) {
    std::cerr << "[WebSocketServer] Invalid handshake" << std::endl;
    if (on_error_) {
      on_error_(conn, "Invalid handshake");
    }
    auto error_resp = handler.GenerateErrorResponse(handler.GetLastError());
    conn->network_conn()->Send(reinterpret_cast<const uint8_t*>(error_resp.data()),
                               error_resp.size());
    return;
  }

  std::string response = handler.GenerateResponse();
  conn->network_conn()->Send(reinterpret_cast<const uint8_t*>(response.data()),
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
      auto net_conn = conn->network_conn();
      if (net_conn) {
        net_conn->Send(close_frame.data(), close_frame.size());
      }
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
        auto net_conn = conn->network_conn();
        if (net_conn) {
          net_conn->Send(close_frame.data(), close_frame.size());
        }
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
