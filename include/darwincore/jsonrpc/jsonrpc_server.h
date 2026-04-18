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
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "darwincore/websocket/websocket_server.h"
#include "darwincore/jsonrpc/notification_builder.h"
#include "darwincore/jsonrpc/request_handler.h"

namespace darwincore {
namespace websocket {

/**
 * @brief JSON-RPC 服务器
 *
 * 建立在 WebSocketServer 之上的 JSON-RPC 2.0 服务器。
 * 复用 WebSocketServer 的握手、帧解析、连接管理等能力。
 */
class JsonRpcServer {
 public:
  JsonRpcServer();
  ~JsonRpcServer();

  // 不可拷贝
  JsonRpcServer(const JsonRpcServer&) = delete;
  JsonRpcServer& operator=(const JsonRpcServer&) = delete;

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
  // JSON-RPC 帧处理
  void HandleJsonRpcFrame(const ConnectionPtr& conn, const Frame& frame);
  void HandleJsonRpcRequest(const ConnectionPtr& conn, const std::string& request);
  bool SendJsonRpcResponse(const ConnectionPtr& conn, const std::string& response);

  std::unique_ptr<::darwincore::jsonrpc::RequestHandler> rpc_handler_;
  std::unique_ptr<WebSocketServer> ws_server_;

  std::atomic<bool> is_running_{false};

  // 回调
  std::function<void(const ConnectionPtr&)> on_connected_;
  std::function<void(const ConnectionPtr&)> on_disconnected_;
  std::function<void(const ConnectionPtr&, const std::string&)> on_error_;
};

}  // namespace websocket
}  // namespace darwincore

#endif  // DARWINCORE_JSONRPC_SERVER_H_
