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
    : rpc_handler_(std::make_unique<jsonrpc::RequestHandler>()) {}

JsonRpcServer::~JsonRpcServer() { Stop(); }

bool JsonRpcServer::Start(const std::string &host, uint16_t port) {
  if (is_running_) {
    return true;
  }

  try {
    // 创建 Server 对象（必须在堆上）
    network_server_ = std::make_unique<network::Server>();

    // 使用原始指针捕获（避免 this 指针问题）
    auto *server_ptr = this;

    // 设置事件回调（必须在 StartIPv4 之前！）
    network_server_->SetOnClientConnected(
        [server_ptr](const network::ConnectionInformation &info) {
          server_ptr->OnNetworkConnected(info);
        });

    network_server_->SetOnMessage(
        [server_ptr](uint64_t connection_id, const std::vector<uint8_t> &data) {
          server_ptr->OnNetworkMessage(connection_id, data);
        });

    network_server_->SetOnClientDisconnected(
        [server_ptr](uint64_t connection_id) {
          server_ptr->OnNetworkDisconnected(connection_id);
        });

    network_server_->SetOnConnectionError(
        [server_ptr](uint64_t connection_id, network::NetworkError error,
                     const std::string &message) {
          server_ptr->OnNetworkError(connection_id, error, message);
        });

    // 启动服务器
    if (!network_server_->StartIPv4(host, port)) {
      return false;
    }

    is_running_ = true;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[ERROR] Exception in Start: " << e.what() << std::endl;
    return false;
  }
}

void JsonRpcServer::Stop() {
  if (!is_running_) {
    return;
  }

  // 第一步：标记服务器为非运行状态（防止新请求被处理）
  is_running_ = false;

  // 第二步：停止网络服务器（停止接受新连接和接收数据）
  if (network_server_) {
    network_server_->Stop();
  }

  // 第三步：清理所有连接并发送关闭帧
  std::vector<uint64_t> connection_ids;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connection_ids.reserve(connections_.size());
    for (const auto &[connection_id, state] : connections_) {
      connection_ids.push_back(connection_id);
    }
    connections_.clear();
  }

  // 第四步：发送关闭帧（网络层已停止，但连接可能还在）
  for (uint64_t connection_id : connection_ids) {
    auto close_frame =
        FrameBuilder::CreateCloseFrame(1000, "Server shutting down");
    if (network_server_) {
      network_server_->SendData(connection_id, close_frame.data(),
                                close_frame.size());
    }
  }
}

bool JsonRpcServer::IsRunning() const { return is_running_; }

void JsonRpcServer::RegisterMethod(const std::string &method,
                                   jsonrpc::MethodHandler handler) {
  rpc_handler_->RegisterMethod(method, std::move(handler));
}

bool JsonRpcServer::SendNotification(uint64_t connection_id,
                                     const std::string &method,
                                     const nlohmann::json &params) {
  // 检查连接是否存在
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    if (connections_.find(connection_id) == connections_.end()) {
      return false; // 连接不存在
    }
  }

  auto notification = jsonrpc::NotificationBuilder::Create(method, params);
  std::vector<uint8_t> payload(notification.begin(), notification.end());

  return SendWebSocketFrame(connection_id, payload, OpCode::kText);
}

void JsonRpcServer::BroadcastNotification(const std::string &method,
                                          const nlohmann::json &params) {
  auto notification = jsonrpc::NotificationBuilder::Create(method, params);
  std::vector<uint8_t> payload(notification.begin(), notification.end());

  // 第一步：在锁内收集所有需要通知的连接 ID
  std::vector<uint64_t> connection_ids;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connection_ids.reserve(connections_.size());
    for (const auto &[connection_id, state] : connections_) {
      if (state->phase == ConnectionPhase::kWebSocket) {
        connection_ids.push_back(connection_id);
      }
    }
  }

  // 第二步：在锁外发送通知，避免死锁
  for (uint64_t connection_id : connection_ids) {
    SendWebSocketFrame(connection_id, payload, OpCode::kText);
  }
}

size_t JsonRpcServer::GetConnectionCount() const {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  return connections_.size();
}

bool JsonRpcServer::CloseConnection(uint64_t connection_id, uint16_t code,
                                    const std::string &reason) {
  return CloseConnectionInternal(connection_id, code, reason, true);
}

bool JsonRpcServer::CloseConnectionInternal(uint64_t connection_id,
                                            uint16_t code,
                                            const std::string &reason,
                                            bool remove_from_map) {
  // 检查连接是否存在
  bool connection_exists = false;
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connection_exists = connections_.find(connection_id) != connections_.end();

    if (connection_exists && remove_from_map) {
      connections_.erase(connection_id);
    }
  }

  // 即使连接不存在也尝试发送关闭帧（让网络层处理）
  auto close_frame = FrameBuilder::CreateCloseFrame(code, reason);
  network_server_->SendData(connection_id, close_frame.data(),
                            close_frame.size());

  return connection_exists;
}

void JsonRpcServer::SetOnClientConnected(
    std::function<void(uint64_t, const network::ConnectionInformation &)>
        callback) {
  on_connected_ = std::move(callback);
}

void JsonRpcServer::SetOnClientDisconnected(
    std::function<void(uint64_t)> callback) {
  on_disconnected_ = std::move(callback);
}

void JsonRpcServer::SetOnError(
    std::function<void(uint64_t, const std::string &)> callback) {
  on_error_ = std::move(callback);
}

void JsonRpcServer::OnNetworkConnected(
    const network::ConnectionInformation &info) {
  // 创建新的连接状态（使用 shared_ptr）
  auto state = std::make_shared<ConnectionState>();
  state->phase = ConnectionPhase::kHandshake;
  state->parser = std::make_unique<FrameParser>();
  state->processed_offset = 0;

  // 先添加连接
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_[info.connection_id] = state;
  }

  // 在锁外调用用户回调，避免死锁
  if (on_connected_) {
    on_connected_(info.connection_id, info);
  }
}

void JsonRpcServer::OnNetworkMessage(uint64_t connection_id,
                                     const std::vector<uint8_t> &data) {
  // 阶段1: 获取连接状态的 shared_ptr（线程安全）
  std::shared_ptr<ConnectionState> state_ptr;
  ConnectionPhase current_phase = ConnectionPhase::kHandshake;
  bool should_close = false;

  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    if (it == connections_.end()) {
      return; // 连接已关闭
    }

    // 拷贝 shared_ptr，增加引用计数（锁外安全使用）
    state_ptr = it->second;
    current_phase = state_ptr->phase;

    // 根据连接阶段检查缓冲区大小限制（防止 DoS）
    size_t current_buffer_size = state_ptr->recv_buffer.size();
    size_t new_buffer_size = current_buffer_size + data.size();

    bool size_valid;
    if (current_phase == ConnectionPhase::kHandshake) {
      size_valid = IsHandshakeSizeValid(new_buffer_size);
    } else {
      // WebSocket 阶段使用更大的限制
      size_valid = (new_buffer_size <= kMaxWebSocketFrameSize);
    }

    if (!size_valid) {
      // 缓冲区过大，标记需要关闭连接（避免嵌套锁死锁）
      connections_.erase(connection_id);
      should_close = true;
    } else {
      // 追加数据到缓冲区
      state_ptr->recv_buffer.insert(state_ptr->recv_buffer.end(), data.begin(),
                                    data.end());
    }
  }

  // 在锁外关闭连接，避免嵌套锁死锁
  if (should_close) {
    CloseConnectionInternal(connection_id, 1009, "Message too big", false);
    return;
  }

  // 阶段2: 根据连接阶段处理数据（锁外执行，避免死锁）
  if (current_phase == ConnectionPhase::kHandshake) {
    bool has_more_data = ProcessHandshakePhase(connection_id, state_ptr);
    // 如果握手成功且缓冲区还有数据，继续处理帧
    if (has_more_data) {
      ProcessWebSocketFramePhase(connection_id, state_ptr);
    }
  } else {
    ProcessWebSocketFramePhase(connection_id, state_ptr);
  }
}

bool JsonRpcServer::ProcessHandshakePhase(
    uint64_t connection_id, std::shared_ptr<ConnectionState> state_ptr) {

  // 查找完整 HTTP 请求结束标记
  size_t request_end = FindHttpRequestEnd(state_ptr->recv_buffer);

  if (request_end == std::string::npos) {
    // 数据不完整，等待更多数据
    return false;
  }

  // 提取完整的握手请求（避免拷贝，使用 string_view 的思想）
  std::string request(state_ptr->recv_buffer.begin(),
                      state_ptr->recv_buffer.begin() + request_end + 4);

  // 锁外执行握手处理（避免死锁）
  bool handshake_success = HandleHandshake(connection_id, request);

  // 更新连接状态（需要持锁）
  {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    if (it == connections_.end()) {
      return false; // 连接已关闭
    }

    if (handshake_success) {
      // 握手成功，移除已处理的握手数据
      state_ptr->recv_buffer.erase(state_ptr->recv_buffer.begin(),
                                   state_ptr->recv_buffer.begin() +
                                       request_end + 4);
      state_ptr->phase = ConnectionPhase::kWebSocket;
      state_ptr->processed_offset = 0; // 重置偏移量

      // 返回是否还有数据需要继续处理
      return !state_ptr->recv_buffer.empty();
    } else {
      // 握手失败，清空缓冲区并关闭连接
      state_ptr->recv_buffer.clear();
      // 使用内部方法，避免重复删除（remove_from_map = false，因为已经在上面的
      // if 块中删除）
      connections_.erase(connection_id);
      CloseConnectionInternal(connection_id, 1002, "Invalid handshake", false);
      return false;
    }
  }
}

void JsonRpcServer::ProcessWebSocketFramePhase(
    uint64_t connection_id, std::shared_ptr<ConnectionState> state_ptr) {

  // 循环解析和处理帧（减少锁持有时间）
  while (true) {
    std::optional<Frame> frame;
    size_t consumed = 0;

    {
      std::lock_guard<std::mutex> lock(connections_mutex_);

      // 简短检查：连接是否还存在
      if (connections_.find(connection_id) == connections_.end()) {
        return; // 连接已关闭
      }

      auto &buffer = state_ptr->recv_buffer;

      // 直接传递指针和长度，避免拷贝
      const uint8_t *data_ptr = buffer.data() + state_ptr->processed_offset;
      size_t data_len = buffer.size() - state_ptr->processed_offset;

      // 构造临时 view 用于解析
      std::vector<uint8_t> parse_view(data_ptr, data_ptr + data_len);
      frame = state_ptr->parser->Parse(parse_view, consumed);

      if (frame.has_value()) {
        // 更新偏移量
        state_ptr->processed_offset += consumed;

        // 定期清理已处理的数据（仅在解析成功后检查）
        if (state_ptr->processed_offset > kBufferCleanupThreshold) {
          size_t erase_len = std::min(state_ptr->processed_offset,
                                      state_ptr->recv_buffer.size());
          state_ptr->recv_buffer.erase(state_ptr->recv_buffer.begin(),
                                       state_ptr->recv_buffer.begin() +
                                           erase_len);
          state_ptr->processed_offset = 0;
        }
      }
    }

    if (!frame.has_value()) {
      // 数据不完整，等待更多数据
      break;
    }

    // 锁外处理帧（避免死锁）
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
                                   const std::string &message) {
  // 调用用户回调
  if (on_error_) {
    on_error_(connection_id, message);
  }
}

bool JsonRpcServer::HandleHandshake(uint64_t connection_id,
                                    const std::string &data) {
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
  network_server_->SendData(connection_id, response_data.data(),
                            response_data.size());

  return true;
}

void JsonRpcServer::HandleWebSocketFrame(uint64_t connection_id,
                                         const Frame &frame) {
  switch (frame.opcode) {
  case OpCode::kText: {
    // 处理文本帧
    std::string request(frame.payload.begin(), frame.payload.end());
    HandleJsonRpcRequest(connection_id, request);
    break;
  }

  case OpCode::kBinary: {
    // 二进制帧直接回显（用于压力测试）
    SendWebSocketFrame(connection_id, frame.payload, OpCode::kBinary);
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
                                       const std::vector<uint8_t> &payload,
                                       OpCode opcode) {
  auto frame = FrameBuilder::BuildFrame(opcode, payload);
  return network_server_->SendData(connection_id, frame.data(), frame.size());
}

void JsonRpcServer::HandleJsonRpcRequest(uint64_t connection_id,
                                         const std::string &request) {
  // 如果服务器正在关闭，忽略新请求
  if (!is_running_) {
    return;
  }

  std::string response = rpc_handler_->HandleRequest(request);

  // 通知不需要响应
  if (response.empty()) {
    return;
  }

  // 发送响应
  std::vector<uint8_t> response_data(response.begin(), response.end());
  SendWebSocketFrame(connection_id, response_data, OpCode::kText);
}

size_t JsonRpcServer::FindHttpRequestEnd(const std::vector<uint8_t> &buffer) {
  /**
   * 在缓冲区中查找 HTTP 请求结束标记 \r\n\r\n
   *
   * @param buffer 数据缓冲区
   * @return 找到返回结束位置，否则返回 std::string::npos
   *
   * 性能优化：直接在 vector<uint8_t> 上查找，避免转换为 string
   */
  if (buffer.size() < 4) {
    return std::string::npos;
  }

  // 查找 \r\n\r\n 模式
  for (size_t i = 0; i <= buffer.size() - 4; ++i) {
    if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' &&
        buffer[i + 3] == '\n') {
      return i;
    }
  }

  return std::string::npos;
}

bool JsonRpcServer::IsHandshakeSizeValid(size_t size) {
  /**
   * 检查握手数据大小是否合法（防止慢速 DoS 攻击）
   *
   * @param size 数据大小
   * @return 合法返回 true
   *
   * RFC 7230 建议最小 8KB，这里设置 16KB 为上限
   */
  return size <= kMaxHandshakeSize;
}

} // namespace websocket
} // namespace darwincore
