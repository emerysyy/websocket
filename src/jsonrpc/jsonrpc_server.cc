// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/jsonrpc/jsonrpc_server.h"

#include <iostream>

namespace darwincore {
namespace websocket {

JsonRpcServer::JsonRpcServer()
    : rpc_handler_(std::make_unique<::darwincore::jsonrpc::RequestHandler>()),
      ws_server_(std::make_unique<WebSocketServer>()) {
  // 设置 WebSocket 回调
  ws_server_->SetOnConnected([this](const ConnectionPtr& conn) {
    if (on_connected_) {
      on_connected_(conn);
    }
  });

  ws_server_->SetOnDisconnected([this](const ConnectionPtr& conn) {
    if (on_disconnected_) {
      on_disconnected_(conn);
    }
  });

  ws_server_->SetOnError([this](const ConnectionPtr& conn, const std::string& error) {
    if (on_error_) {
      on_error_(conn, error);
    }
  });

  // 设置帧回调，拦截 JSON-RPC 请求
  ws_server_->SetOnFrame([this](const ConnectionPtr& conn, const Frame& frame) {
    HandleJsonRpcFrame(conn, frame);
  });
}

JsonRpcServer::~JsonRpcServer() { Stop(); }

bool JsonRpcServer::Start(const std::string& host, uint16_t port) {
  if (is_running_.load()) {
    return false;
  }

  if (!ws_server_->Start(host, port)) {
    return false;
  }

  is_running_.store(true);
  return true;
}

void JsonRpcServer::Stop() {
  if (!is_running_.load()) {
    return;
  }

  is_running_.store(false);
  ws_server_->Stop();
}

bool JsonRpcServer::IsRunning() const { return is_running_.load(); }

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
  return ws_server_->SendText(conn, notification);
}

void JsonRpcServer::BroadcastNotification(const std::string& method,
                                         const nlohmann::json& params) {
  auto notification = ::darwincore::jsonrpc::NotificationBuilder::Create(method, params);
  std::vector<uint8_t> payload(notification.begin(), notification.end());
  ws_server_->Broadcast(payload, OpCode::kText);
}

size_t JsonRpcServer::GetConnectionCount() const {
  return ws_server_->GetConnectionCount();
}

bool JsonRpcServer::CloseConnection(const ConnectionPtr& conn, uint16_t code,
                                    const std::string& reason) {
  if (!conn || !conn->IsConnected()) {
    return false;
  }

  // 使用 ForceClose 保持同步关闭行为
  // 旧行为：调用后立即断开并触发 on_disconnected_
  ws_server_->ForceClose(conn);
  return true;
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

void JsonRpcServer::HandleJsonRpcFrame(const ConnectionPtr& conn,
                                      const Frame& frame) {
  if (!is_running_ || !conn) {
    return;
  }

  // 只处理文本帧
  if (frame.opcode != OpCode::kText) {
    return;
  }

  std::string request(frame.payload.begin(), frame.payload.end());
  HandleJsonRpcRequest(conn, request);
}

void JsonRpcServer::HandleJsonRpcRequest(const ConnectionPtr& conn,
                                        const std::string& request) {
  std::string response = rpc_handler_->HandleRequest(request);

  if (!response.empty()) {
    SendJsonRpcResponse(conn, response);
  }
}

bool JsonRpcServer::SendJsonRpcResponse(const ConnectionPtr& conn,
                                       const std::string& response) {
  if (!conn || !conn->IsConnected()) {
    return false;
  }

  return ws_server_->SendText(conn, response);
}

}  // namespace websocket
}  // namespace darwincore
