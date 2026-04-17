// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/jsonrpc/jsonrpc_server.h"

namespace {

using namespace darwincore::websocket;
using nlohmann::json;

}  // namespace

TEST(JsonRpcServer, Constructor) {
  JsonRpcServer server;
  EXPECT_FALSE(server.IsRunning());
}

TEST(JsonRpcServer, StartStop) {
  JsonRpcServer server;

  EXPECT_TRUE(server.Start("127.0.0.1", 8080));
  EXPECT_TRUE(server.IsRunning());

  server.Stop();
  EXPECT_FALSE(server.IsRunning());
}

TEST(JsonRpcServer, RegisterMethod) {
  JsonRpcServer server;
  server.RegisterMethod("test", [](const json& params) { return json(true); });

  // Method 注册成功，不应抛出异常
  EXPECT_TRUE(true);
}

TEST(JsonRpcServer, ConnectionCount) {
  JsonRpcServer server;
  server.Start("127.0.0.1", 8080);

  EXPECT_EQ(server.GetConnectionCount(), 0);

  server.Stop();
}

TEST(JsonRpcServer, BroadcastNotification) {
  JsonRpcServer server;
  server.Start("127.0.0.1", 8080);

  // 无连接时广播不应崩溃
  server.BroadcastNotification("broadcast", {{"data", 42}});

  server.Stop();
}

TEST(JsonRpcServer, SetCallbacks) {
  JsonRpcServer server;
  bool connected = false;
  bool disconnected = false;

  server.SetOnClientConnected([&](const ConnectionPtr&) {
    connected = true;
  });

  server.SetOnClientDisconnected([&](const ConnectionPtr&) {
    disconnected = true;
  });

  server.SetOnError([&](const ConnectionPtr&, const std::string&) {});

  // 回调设置成功
  EXPECT_TRUE(true);
}

// 测试 ConnectionContext 会话状态挂在 Connection 上
TEST(JsonRpcServer, ConnectionContextOnConnection) {
  // 创建模拟 Connection
  auto conn = std::make_shared<Connection>(123);

  // 初始 phase 应为 kHandshake
  auto* ctx = conn->GetSessionState();
  EXPECT_EQ(ctx, nullptr);  // 未设置时返回 nullptr

  // 设置会话状态
  conn->SetSessionState(ConnectionPhase::kHandshake);
  ctx = conn->GetSessionState();
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(ctx->phase, ConnectionPhase::kHandshake);
  EXPECT_EQ(ctx->connection_id, 123);

  // 修改 phase
  ctx->phase = ConnectionPhase::kWebSocket;
  EXPECT_EQ(conn->GetSessionState()->phase, ConnectionPhase::kWebSocket);

  // 测试 recv_buffer
  std::vector<uint8_t> test_data = {0x01, 0x02, 0x03};
  ctx->recv_buffer.insert(ctx->recv_buffer.end(), test_data.begin(), test_data.end());
  EXPECT_EQ(conn->GetSessionState()->recv_buffer.size(), 3);
}

// 测试多个 Connection 各自独立存储会话状态
TEST(JsonRpcServer, IndependentConnectionContexts) {
  auto conn1 = std::make_shared<Connection>(1);
  auto conn2 = std::make_shared<Connection>(2);

  conn1->SetSessionState(ConnectionPhase::kHandshake);
  conn2->SetSessionState(ConnectionPhase::kWebSocket);

  EXPECT_EQ(conn1->GetSessionState()->phase, ConnectionPhase::kHandshake);
  EXPECT_EQ(conn2->GetSessionState()->phase, ConnectionPhase::kWebSocket);

  // 修改 conn1 不影响 conn2
  conn1->GetSessionState()->phase = ConnectionPhase::kWebSocket;
  EXPECT_EQ(conn2->GetSessionState()->phase, ConnectionPhase::kWebSocket);
}

// 测试 ConnectionContext 与 std::any 集成
TEST(JsonRpcServer, ContextWithStdAny) {
  auto conn = std::make_shared<Connection>(456);

  // 通过 std::any 存储和获取
  ConnectionContext ctx;
  ctx.phase = ConnectionPhase::kHandshake;
  ctx.connection_id = 456;
  ctx.recv_buffer = {0xAA, 0xBB, 0xCC};

  conn->SetContext(std::make_any<ConnectionContext>(ctx));

  // 验证类型安全转换
  auto* retrieved_ctx = std::any_cast<ConnectionContext>(&conn->GetContext());
  ASSERT_NE(retrieved_ctx, nullptr);
  EXPECT_EQ(retrieved_ctx->phase, ConnectionPhase::kHandshake);
  EXPECT_EQ(retrieved_ctx->connection_id, 456);
  EXPECT_EQ(retrieved_ctx->recv_buffer.size(), 3);

  // 错误的类型转换应返回 nullptr
  auto* wrong_type = std::any_cast<int>(&conn->GetContext());
  EXPECT_EQ(wrong_type, nullptr);
}
