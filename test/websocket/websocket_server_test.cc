

// Copyright (C) 2025
// Licensed under the MIT License

#define WEBSOCKET_SERVER_TEST

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "darwincore/websocket/websocket_server.h"
#include <darwincore/network/client.h>

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

// 测试 Close 后再 ForceClose 不应重复发送 Close 帧
TEST(WebSocketServer, ForceCloseAfterCloseIsIdempotent) {
  WebSocketServer server;
  server.Start("127.0.0.1", 0);

  int disconnect_count = 0;
  server.SetOnDisconnected([&](const ConnectionPtr&) {
    ++disconnect_count;
  });

  auto conn = server.AddTestConnection(305, "10.0.0.6:8080");

  EXPECT_TRUE(server.Close(conn, 1000, "Normal closure"));
  EXPECT_TRUE(conn->is_closing());

  server.ForceClose(conn);
  EXPECT_EQ(disconnect_count, 1);
  EXPECT_FALSE(conn->IsConnected());
  EXPECT_EQ(server.GetConnectionCount(), 0);

  // 再次调用仍应保持幂等
  server.ForceClose(conn);
  EXPECT_EQ(disconnect_count, 1);

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

// ========== 集成测试：网络驱动的断开路径 ==========

// 测试：客户端主动断开连接，服务器 on_disconnected_ 被调用
TEST(WebSocketServer, ClientDisconnectTriggersOnDisconnected) {
  WebSocketServer server;
  server.Start("127.0.0.1", 18888);

  bool server_disconnect_called = false;
  ConnectionPtr disconnected_conn;

  server.SetOnDisconnected([&](const ConnectionPtr& conn) {
    server_disconnect_called = true;
    disconnected_conn = conn;
  });

  // 连接计数初始为 0
  EXPECT_EQ(server.GetConnectionCount(), 0);

  // 客户端连接
  darwincore::network::Client client;
  bool client_connected = false;

  client.SetOnConnected([&](const darwincore::network::ConnectionInformation&) {
    client_connected = true;
  });

  EXPECT_TRUE(client.ConnectIPv4("127.0.0.1", 18888));

  // 等待连接建立
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_TRUE(client_connected);
  EXPECT_EQ(server.GetConnectionCount(), 1);  // 服务器感知到连接

  // 客户端断开连接
  client.Disconnect();

  // 等待服务器处理断开
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // 验证服务器收到断开回调
  EXPECT_TRUE(server_disconnect_called);
  EXPECT_EQ(server.GetConnectionCount(), 0);  // 连接计数减少

  server.Stop();
}

// 测试：服务器调用 ForceClose，清理服务器端状态
// 注意：ForceClose 只清理 WebSocket 层，不关闭 TCP 连接
// 客户端需要主动 Disconnect() 才能触发服务器端的 on_disconnected_
TEST(WebSocketServer, ForceCloseCleansServerState) {
  WebSocketServer server;
  server.Start("127.0.0.1", 18889);

  ConnectionPtr server_conn;
  server.SetOnConnected([&](const ConnectionPtr& conn) {
    server_conn = conn;
  });

  darwincore::network::Client client;

  EXPECT_TRUE(client.ConnectIPv4("127.0.0.1", 18889));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_TRUE(server_conn != nullptr);
  EXPECT_EQ(server.GetConnectionCount(), 1);

  // ForceClose 清理服务器端状态
  server.ForceClose(server_conn);

  EXPECT_EQ(server.GetConnectionCount(), 0);
  EXPECT_FALSE(server_conn->IsConnected());

  // 客户端仍需主动断开（服务器不会自动关闭客户端连接）
  client.Disconnect();

  server.Stop();
}

// 测试：多个客户端连接和断开
TEST(WebSocketServer, MultipleClientsConnectAndDisconnect) {
  WebSocketServer server;
  server.Start("127.0.0.1", 18890);

  int disconnect_count = 0;
  server.SetOnDisconnected([&](const ConnectionPtr&) {
    ++disconnect_count;
  });

  int connected_count = 0;
  server.SetOnConnected([&](const ConnectionPtr&) {
    ++connected_count;
  });

  // 连接 3 个客户端
  darwincore::network::Client client1, client2, client3;

  EXPECT_TRUE(client1.ConnectIPv4("127.0.0.1", 18890));
  EXPECT_TRUE(client2.ConnectIPv4("127.0.0.1", 18890));
  EXPECT_TRUE(client3.ConnectIPv4("127.0.0.1", 18890));

  // 等待所有连接建立
  while (connected_count < 3) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  EXPECT_EQ(server.GetConnectionCount(), 3);

  // 断开第一个客户端
  client1.Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(server.GetConnectionCount(), 2);
  EXPECT_EQ(disconnect_count, 1);

  // 断开第二个客户端
  client2.Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(server.GetConnectionCount(), 1);
  EXPECT_EQ(disconnect_count, 2);

  // 断开第三个客户端
  client3.Disconnect();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(server.GetConnectionCount(), 0);
  EXPECT_EQ(disconnect_count, 3);

  server.Stop();
}

// 测试：客户端异常断开（析构时自动断开）
TEST(WebSocketServer, ClientDestructorTriggersOnDisconnected) {
  WebSocketServer server;
  server.Start("127.0.0.1", 18891);

  int disconnect_count = 0;
  server.SetOnDisconnected([&](const ConnectionPtr&) {
    ++disconnect_count;
  });

  EXPECT_EQ(server.GetConnectionCount(), 0);

  // 在作用域内创建客户端并连接
  {
    darwincore::network::Client client;
    client.ConnectIPv4("127.0.0.1", 18891);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(server.GetConnectionCount(), 1);
  }  // client 析构，自动断开

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  EXPECT_EQ(disconnect_count, 1);
  EXPECT_EQ(server.GetConnectionCount(), 0);

  server.Stop();
}
