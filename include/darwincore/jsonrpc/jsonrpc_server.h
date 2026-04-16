// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_JSONRPC_SERVER_H_
#define DARWINCORE_JSONRPC_SERVER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "darwincore/websocket/frame_builder.h"
#include "darwincore/websocket/handshake_handler.h"
#include "darwincore/jsonrpc/notification_builder.h"
#include "darwincore/jsonrpc/request_handler.h"

namespace darwincore {
namespace websocket {

/**
 * @brief 连接信息
 */
struct ConnectionInformation {
  uint64_t connection_id = 0;
  std::string remote_address;
  uint16_t remote_port = 0;
};

/**
 * @brief 连接阶段
 */
enum class ConnectionPhase {
  kHandshake,
  kWebSocket,
};

/**
 * @brief 连接状态
 */
struct ConnectionState {
  ConnectionPhase phase = ConnectionPhase::kHandshake;
  std::vector<uint8_t> recv_buffer;
  size_t processed_offset = 0;
};

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
   * @param connection_id 连接 ID
   * @param method 方法名
   * @param params 参数
   * @return 成功返回 true
   */
  bool SendNotification(uint64_t connection_id, const std::string& method,
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
   * @param connection_id 连接 ID
   * @param code 关闭码
   * @param reason 关闭原因
   */
  bool CloseConnection(uint64_t connection_id, uint16_t code = 1000,
                       const std::string& reason = "");

  /**
   * @brief 设置客户端连接回调
   */
  void SetOnClientConnected(
      std::function<void(uint64_t, const ConnectionInformation&)> callback);

  /**
   * @brief 设置客户端断开回调
   */
  void SetOnClientDisconnected(std::function<void(uint64_t)> callback);

  /**
   * @brief 设置错误回调
   */
  void SetOnError(std::function<void(uint64_t, const std::string&)> callback);

 private:
  void OnNetworkConnected(const ConnectionInformation& info);
  void OnNetworkMessage(uint64_t connection_id, const std::vector<uint8_t>& data);
  void OnNetworkDisconnected(uint64_t connection_id);
  void OnNetworkError(uint64_t connection_id, const std::string& message);

  bool HandleHandshake(uint64_t connection_id, const std::string& data);
  void HandleWebSocketFrame(uint64_t connection_id, const Frame& frame);
  void HandleJsonRpcRequest(uint64_t connection_id, const std::string& request);
  bool SendWebSocketFrame(uint64_t connection_id,
                          const std::vector<uint8_t>& payload,
                          OpCode opcode);

  size_t FindHttpRequestEnd(const std::vector<uint8_t>& buffer);
  bool IsHandshakeSizeValid(size_t size);

  static constexpr size_t kMaxHandshakeSize = 16384;
  static constexpr size_t kMaxWebSocketFrameSize = 10 * 1024 * 1024;
  static constexpr size_t kBufferCleanupThreshold = 4096;

  std::unique_ptr<::darwincore::jsonrpc::RequestHandler> rpc_handler_;

  std::atomic<bool> is_running_{false};

  // 连接管理
  mutable std::mutex connections_mutex_;
  std::unordered_map<uint64_t, std::shared_ptr<ConnectionState>> connections_;

  // 回调
  std::function<void(uint64_t, const ConnectionInformation&)> on_connected_;
  std::function<void(uint64_t)> on_disconnected_;
  std::function<void(uint64_t, const std::string&)> on_error_;

  // 内部实现
  bool CloseConnectionInternal(uint64_t connection_id, uint16_t code,
                               const std::string& reason, bool remove_from_map);
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_JSONRPC_SERVER_H_
