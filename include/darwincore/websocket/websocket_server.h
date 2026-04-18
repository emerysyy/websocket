// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_WEBSOCKET_SERVER_H_
#define DARWINCORE_WEBSOCKET_SERVER_H_

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

#include <darwincore/network/server.h>

#include "frame_builder.h"
#include "frame_parser.h"
#include "handshake_handler.h"
#include "session.h"

namespace darwincore {
namespace websocket {

/**
 * @brief 连接包装器（WebSocket 层使用）
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
  void ConsumeRecvBuffer(size_t bytes) {
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
 * @brief WebSocket 帧回调
 *
 * @param conn 连接指针
 * @param frame 解析后的帧
 */
using FrameCallback = std::function<void(const ConnectionPtr& conn, const Frame& frame)>;

/**
 * @brief WebSocket 服务器
 *
 * 统一管理 WebSocket 服务器，处理 HTTP Upgrade 握手和 WebSocket 帧解析。
 * 会话状态挂在 Connection 自身（继承 WebSocketSession），不维护全局连接表。
 *
 * 使用示例：
 * @code
 * WebSocketServer server;
 * server.SetOnFrame([](const ConnectionPtr& conn, const Frame& frame) {
 *   if (frame.opcode == OpCode::kText) {
 *     // 处理文本帧
 *     std::string payload(frame.payload.begin(), frame.payload.end());
 *     std::cout << "Received: " << payload << std::endl;
 *
 *     // 回复
 *     auto response = FrameBuilder::BuildFrame(OpCode::kText, frame.payload);
 *     server.SendFrame(conn, response);
 *   }
 * });
 * server.Start("0.0.0.0", 8080);
 * @endcode
 */
class WebSocketServer {
 public:
  WebSocketServer();
  ~WebSocketServer();

  // 不可拷贝
  WebSocketServer(const WebSocketServer&) = delete;
  WebSocketServer& operator=(const WebSocketServer&) = delete;

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
   * @brief 获取当前连接数
   */
  size_t GetConnectionCount() const;

  /**
   * @brief 发送 WebSocket 帧到指定连接
   * @param conn 连接指针
   * @param payload 负载数据
   * @param opcode 操作码（默认 kText）
   * @return 成功返回 true
   */
  bool SendFrame(const ConnectionPtr& conn,
                 const std::vector<uint8_t>& payload,
                 OpCode opcode = OpCode::kText);

  /**
   * @brief 发送文本帧到指定连接
   * @param conn 连接指针
   * @param text 文本内容
   * @return 成功返回 true
   */
  bool SendText(const ConnectionPtr& conn, const std::string& text);

  /**
   * @brief 发送二进制帧到指定连接
   * @param conn 连接指针
   * @param data 二进制数据
   * @return 成功返回 true
   */
  bool SendBinary(const ConnectionPtr& conn, const std::vector<uint8_t>& data);

  /**
   * @brief 发送 Ping 帧
   * @param conn 连接指针
   * @param payload 可选的 ping 数据
   * @return 成功返回 true
   */
  bool SendPing(const ConnectionPtr& conn, const std::vector<uint8_t>& payload = {});

  /**
   * @brief 发送 Pong 帧
   * @param conn 连接指针
   * @param payload pong 数据（通常回显 ping 的 payload）
   * @return 成功返回 true
   */
  bool SendPong(const ConnectionPtr& conn, const std::vector<uint8_t>& payload = {});

  /**
   * @brief 发送关闭帧并关闭连接
   * @param conn 连接指针
   * @param code 关闭码（默认 1000 正常关闭）
   * @param reason 关闭原因
   * @return 成功返回 true
   */
  bool Close(const ConnectionPtr& conn, uint16_t code = 1000,
             const std::string& reason = "");

  /**
   * @brief 强制立即关闭连接（同步关闭）
   *
   * 不会发送 Close 帧，直接断开连接并触发 on_disconnected_。
   * 适用于服务器主动终止连接的场景。
   *
   * @param conn 连接指针
   */
  void ForceClose(const ConnectionPtr& conn);

  /**
   * @brief 广播帧到所有连接
   * @param payload 负载数据
   * @param opcode 操作码
   * @return 发送的连接数
   */
  size_t Broadcast(const std::vector<uint8_t>& payload, OpCode opcode = OpCode::kText);

  /**
   * @brief 设置帧回调
   * @param callback 帧处理回调
   */
  void SetOnFrame(FrameCallback callback);

  /**
   * @brief 设置客户端连接回调
   * @param callback 连接建立回调
   */
  void SetOnConnected(std::function<void(const ConnectionPtr&)> callback);

  /**
   * @brief 设置客户端断开回调
   * @param callback 连接断开回调
   */
  void SetOnDisconnected(std::function<void(const ConnectionPtr&)> callback);

  /**
   * @brief 设置错误回调
   * @param callback 错误处理回调
   */
  void SetOnError(std::function<void(const ConnectionPtr&, const std::string&)> callback);

 private:
  // 辅助方法
  ConnectionPtr GetConnection(uint64_t connection_id);
  size_t FindHttpRequestEnd(const std::vector<uint8_t>& buffer);

  // DarwinCore 回调处理
  void OnNetworkConnected(const darwincore::network::ConnectionInformation& info);
  void OnNetworkMessage(uint64_t connection_id, const std::vector<uint8_t>& data);
  void OnNetworkDisconnected(uint64_t connection_id);

  // 协议处理
  void HandleHandshake(const ConnectionPtr& conn, const std::string& data);
  void ProcessWebSocketFrames(const ConnectionPtr& conn);
  void HandleFrame(const ConnectionPtr& conn, const Frame& frame);

  // 常量
  static constexpr size_t kMaxHandshakeSize = 16384;
  static constexpr size_t kBufferCleanupThreshold = 4096;

  std::atomic<bool> is_running_{false};

  // 连接索引（仅用于 connection_id 到 ConnectionPtr 的映射）
  mutable std::mutex connections_mutex_;
  std::unordered_map<uint64_t, ConnectionPtr> connections_;

  // DarwinCore 网络服务器
  std::unique_ptr<darwincore::network::Server> network_server_;

  // 回调
  FrameCallback on_frame_;
  std::function<void(const ConnectionPtr&)> on_connected_;
  std::function<void(const ConnectionPtr&)> on_disconnected_;
  std::function<void(const ConnectionPtr&, const std::string&)> on_error_;
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_WEBSOCKET_SERVER_H_
