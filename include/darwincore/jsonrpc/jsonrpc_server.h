// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_JSONRPC_SERVER_H_
#define DARWINCORE_JSONRPC_SERVER_H_

#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <darwincore/network/server.h>

#include "darwincore/websocket/frame_builder.h"
#include "darwincore/websocket/frame_parser.h"
#include "darwincore/websocket/handshake_handler.h"
#include "darwincore/websocket/session.h"
#include "darwincore/jsonrpc/notification_builder.h"
#include "darwincore/jsonrpc/request_handler.h"

namespace darwincore {
namespace websocket {

/**
 * @brief 连接包装器（业务层使用）
 *
 * 封装 connection_id，继承 WebSocketSession 提供会话状态管理。
 * 会话状态挂在 Connection 自身（通过继承 WebSocketSession），
 * 不维护全局连接表。
 *
 * 遵循架构原则：
 * - 一个连接对应一个 session
 * - session 状态挂在 Connection 上
 */
class Connection final : public WebSocketSession {
 public:
  explicit Connection(uint64_t id, const std::string& remote_addr = "")
      : WebSocketSession(), connection_id_(id), remote_address_(remote_addr) {
    set_phase(SessionPhase::kHandshake);
  }

  uint64_t connection_id() const { return connection_id_; }
  const std::string& remote_address() const { return remote_address_; }
  bool IsConnected() const { return connected_; }
  void set_connected(bool connected) { connected_ = connected; }

  // 接收缓冲区（跨帧缓冲未完整数据）
  std::vector<uint8_t>& recv_buffer() { return recv_buffer_; }
  const std::vector<uint8_t>& recv_buffer() const { return recv_buffer_; }

  // 移除已处理的字节
  void consume_recv_buffer(size_t bytes) {
    if (bytes >= recv_buffer_.size()) {
      recv_buffer_.clear();
    } else {
      recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + bytes);
    }
  }

 private:
  uint64_t connection_id_;
  std::string remote_address_;
  bool connected_ = true;
  std::vector<uint8_t> recv_buffer_;  // 跨帧缓冲
};

/**
 * @brief 连接指针类型
 */
using ConnectionPtr = std::shared_ptr<Connection>;

/**
 * @brief JSON-RPC 服务器
 *
 * 建立在 WebSocket 层之上的 JSON-RPC 2.0 服务器。
 * 会话状态挂在 Connection::context_ 上，不维护全局连接表。
 */
class JsonRpcServer {
 public:
  JsonRpcServer();
  ~JsonRpcServer();

  /**
   * @brief 启动服务器
   * @param host 监听地址
   * @param port 监听端口
   * @return 成功返回 true
   */
  bool Start(const std::string& host, uint16_t port);

  /**
   * @brief 停止服务器
   */
  void Stop();

  /**
   * @brief 检查服务器是否运行中
   */
  bool IsRunning() const;

  /**
   * @brief 注册 JSON-RPC 方法
   * @param method 方法名
   * @param handler 处理函数
   */
  void RegisterMethod(const std::string& method, ::darwincore::jsonrpc::MethodHandler handler);

  /**
   * @brief 发送通知到指定连接
   * @param conn 连接指针
   * @param method 方法名
   * @param params 参数
   * @return 成功返回 true
   */
  bool SendNotification(const ConnectionPtr& conn, const std::string& method,
                       const nlohmann::json& params);

  /**
   * @brief 广播通知到所有连接
   * @param method 方法名
   * @param params 参数
   */
  void BroadcastNotification(const std::string& method,
                            const nlohmann::json& params);

  /**
   * @brief 获取当前连接数
   */
  size_t GetConnectionCount() const;

  /**
   * @brief 关闭指定连接
   * @param conn 连接指针
   * @param code 关闭码
   * @param reason 关闭原因
   */
  bool CloseConnection(const ConnectionPtr& conn, uint16_t code = 1000,
                       const std::string& reason = "");

  /**
   * @brief 设置客户端连接回调
   */
  void SetOnClientConnected(
      std::function<void(const ConnectionPtr&)> callback);

  /**
   * @brief 设置客户端断开回调
   */
  void SetOnClientDisconnected(std::function<void(const ConnectionPtr&)> callback);

  /**
   * @brief 设置错误回调
   */
  void SetOnError(std::function<void(const ConnectionPtr&, const std::string&)> callback);

 private:
  // 辅助 - 通过 connection_id 查找 ConnectionPtr
  // 注意：connections_ 只用于 id → ConnectionPtr 的反向查找
  // 会话状态挂在 Connection 自身（继承 WebSocketSession）
  ConnectionPtr GetConnection(uint64_t connection_id);
  size_t FindHttpRequestEnd(const std::vector<uint8_t>& buffer);
  bool IsHandshakeSizeValid(size_t size);

  // 内部实现
  void CloseConnectionInternal(const ConnectionPtr& conn, uint16_t code,
                               const std::string& reason);

  // DarwinCore 回调处理
  void OnNetworkConnected(const darwincore::network::ConnectionInformation& info);
  void OnNetworkMessage(uint64_t connection_id, const std::vector<uint8_t>& data);
  void OnNetworkDisconnected(uint64_t connection_id);
  void OnNetworkError(uint64_t connection_id, const std::string& message);

  // 协议处理
  void HandleHandshake(const ConnectionPtr& conn, const std::string& data);
  void ProcessWebSocketFrames(const ConnectionPtr& conn);
  void HandleWebSocketFrame(const ConnectionPtr& conn, const Frame& frame);
  void HandleJsonRpcRequest(const ConnectionPtr& conn, const std::string& request);
  bool SendWebSocketFrame(const ConnectionPtr& conn,
                         const std::vector<uint8_t>& payload,
                         OpCode opcode);

  static constexpr size_t kMaxHandshakeSize = 16384;
  static constexpr size_t kMaxWebSocketFrameSize = 10 * 1024 * 1024;
  static constexpr size_t kBufferCleanupThreshold = 4096;

  std::unique_ptr<::darwincore::jsonrpc::RequestHandler> rpc_handler_;

  std::atomic<bool> is_running_{false};

  // 连接索引（仅用于 connection_id 到 ConnectionPtr 的映射）
  mutable std::mutex connections_mutex_;
  std::unordered_map<uint64_t, ConnectionPtr> connections_;

  // DarwinCore 网络服务器
  std::unique_ptr<darwincore::network::Server> network_server_;

  // 回调
  std::function<void(const ConnectionPtr&)> on_connected_;
  std::function<void(const ConnectionPtr&)> on_disconnected_;
  std::function<void(const ConnectionPtr&, const std::string&)> on_error_;
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_JSONRPC_SERVER_H_
