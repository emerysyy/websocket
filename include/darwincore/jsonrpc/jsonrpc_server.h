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
#include "darwincore/websocket/handshake_handler.h"
#include "darwincore/jsonrpc/notification_builder.h"
#include "darwincore/jsonrpc/request_handler.h"

namespace darwincore {
namespace websocket {

/**
 * @brief 连接阶段
 */
enum class ConnectionPhase {
  kHandshake,
  kWebSocket,
};

/**
 * @brief 连接上下文（存储在 std::any 中）
 *
 * 遵循架构原则：会话状态挂在连接上，不维护全局连接表。
 */
struct ConnectionContext {
  ConnectionPhase phase = ConnectionPhase::kHandshake;
  std::vector<uint8_t> recv_buffer;
  size_t processed_offset = 0;
  uint64_t connection_id = 0;
  std::string remote_address;
};

/**
 * @brief 连接包装器（业务层使用）
 *
 * 封装 connection_id，提供更高层的抽象。
 * 将来 DarwinCore 支持 ConnectionPtr 时可直接替换。
 */
class Connection final {
 public:
  explicit Connection(uint64_t id) : connection_id_(id) {}

  uint64_t connection_id() const { return connection_id_; }
  bool IsConnected() const { return connected_; }
  void set_connected(bool connected) { connected_ = connected; }

  // 上下文管理（std::any 存储会话状态）
  void SetContext(std::any ctx) { context_ = std::move(ctx); }
  std::any& GetContext() { return context_; }
  const std::any& GetContext() const { return context_; }

  // 获取会话状态
  ConnectionContext* GetSessionState() {
    return std::any_cast<ConnectionContext>(&context_);
  }

  // 设置会话状态
  void SetSessionState(ConnectionPhase phase) {
    ConnectionContext ctx;
    ctx.phase = phase;
    ctx.connection_id = connection_id_;
    context_ = std::make_any<ConnectionContext>(ctx);
  }

 private:
  uint64_t connection_id_;
  bool connected_ = true;
  std::any context_;
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
  // 内部连接状态（挂在 Connection::context_ 上）
  struct SessionState {
    ConnectionPhase phase = ConnectionPhase::kHandshake;
    std::vector<uint8_t> recv_buffer;
    size_t processed_offset = 0;
  };

  // 辅助
  SessionState* GetOrCreateSession(const ConnectionPtr& conn);
  SessionState* GetSession(const ConnectionPtr& conn);
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
