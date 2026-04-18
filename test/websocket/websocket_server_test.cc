// Copyright (C) 2025
// Licensed under the MIT License

#define WEBSOCKET_SERVER_TEST

#include <gtest/gtest.h>
#include "darwincore/websocket/websocket_server.h"

namespace {

using namespace darwincore::websocket;

}  // namespace

// 测试 Broadcast 过滤握手阶段连接
TEST(WebSocketServer, BroadcastFiltersHandshakeConnections) {
  WebSocketServer server;
  server.Start("127.0.0.1", 0);  // port 0 表示自动选择

  // 添加一个在握手阶段的连接（模拟连接建立但未完成升级）
  auto handshake_conn = server.AddTestConnection(1, "192.168.1.1:12345");
  handshake_conn->set_phase(SessionPhase::kHandshake);
  EXPECT_EQ(handshake_conn->phase(), SessionPhase::kHandshake);

  // 添加一个已完成升级的连接
  auto ws_conn = server.AddTestConnection(2, "192.168.1.2:12345");
  ws_conn->set_phase(SessionPhase::kWebSocket);
  EXPECT_EQ(ws_conn->phase(), SessionPhase::kWebSocket);

  // 广播应该只发给 WebSocket 阶段的连接
  std::vector<uint8_t> payload = {'h', 'e', 'l', 'l', 'o'};
  size_t count = server.Broadcast(payload, OpCode::kText);

  // 只有 ws_conn 会被包含，handshake_conn 会被过滤
  EXPECT_EQ(count, 1);

  server.Stop();
}

// 测试 ForceClose 立即触发断开回调
TEST(WebSocketServer, ForceCloseTriggersDisconnectCallback) {
  WebSocketServer server;
  server.Start("127.0.0.1", 0);

  bool disconnect_called = false;
  ConnectionPtr disconnected_conn;

  server.SetOnDisconnected([&](const ConnectionPtr& conn) {
    disconnect_called = true;
    disconnected_conn = conn;
  });

  // 添加一个连接
  auto conn = server.AddTestConnection(100, "10.0.0.1:8080");
  conn->set_phase(SessionPhase::kWebSocket);

  EXPECT_TRUE(conn->IsConnected());
  EXPECT_EQ(server.GetConnectionCount(), 1);

  // ForceClose 应该立即触发回调
  server.ForceClose(conn);

  EXPECT_TRUE(disconnect_called);
  EXPECT_EQ(disconnected_conn, conn);
  EXPECT_FALSE(conn->IsConnected());
  EXPECT_EQ(server.GetConnectionCount(), 0);  // 连接已从列表中移除

  // ForceClose 幂等：再次调用不应崩溃
  server.ForceClose(conn);

  server.Stop();
}

// 测试 ForceClose 幂等性
TEST(WebSocketServer, ForceCloseIdempotent) {
  WebSocketServer server;
  server.Start("127.0.0.1", 0);

  int disconnect_count = 0;
  server.SetOnDisconnected([&](const ConnectionPtr&) {
    ++disconnect_count;
  });

  auto conn = server.AddTestConnection(200, "10.0.0.2:8080");

  // 第一次 ForceClose
  server.ForceClose(conn);
  EXPECT_EQ(disconnect_count, 1);
  EXPECT_FALSE(conn->IsConnected());

  // 第二次 ForceClose（幂等）
  server.ForceClose(conn);
  EXPECT_EQ(disconnect_count, 1);  // 不应再次触发回调

  server.Stop();
}

// 测试 Close (异步) 不会立即触发断开回调
TEST(WebSocketServer, CloseDoesNotTriggerImmediateDisconnect) {
  WebSocketServer server;
  server.Start("127.0.0.1", 0);

  bool disconnect_called = false;
  server.SetOnDisconnected([&](const ConnectionPtr&) {
    disconnect_called = true;
  });

  auto conn = server.AddTestConnection(300, "10.0.0.3:8080");

  // Close() 发送 Close 帧，但不应该立即触发断开回调
  bool close_result = server.Close(conn, 1000, "Normal closure");

  EXPECT_TRUE(close_result);
  EXPECT_FALSE(disconnect_called);  // 不应立即触发
  EXPECT_TRUE(conn->is_closing());
  EXPECT_EQ(server.GetConnectionCount(), 1);  // 连接仍在列表中

  // ForceClose 用于测试场景：清理连接
  server.ForceClose(conn);

  server.Stop();
}

// 测试 Broadcast 不包含正在关闭的连接
TEST(WebSocketServer, BroadcastExcludesClosingConnections) {
  WebSocketServer server;
  server.Start("127.0.0.1", 0);

  auto conn1 = server.AddTestConnection(401, "10.0.0.4:8080");
  conn1->set_phase(SessionPhase::kWebSocket);

  auto conn2 = server.AddTestConnection(402, "10.0.0.5:8080");
  conn2->set_phase(SessionPhase::kWebSocket);
  conn2->set_closing(true);  // 标记为正在关闭

  std::vector<uint8_t> payload = {'t', 'e', 's', 't'};
  size_t count = server.Broadcast(payload, OpCode::kText);

  // conn2 正在关闭，不应被包含
  EXPECT_EQ(count, 1);

  server.Stop();
}
