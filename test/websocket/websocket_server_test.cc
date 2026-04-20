// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>

#include <vector>

#include "darwincore/websocket/websocket_server.h"

namespace {

using namespace darwincore::websocket;

class WebSocketServerTest : public ::testing::Test {
 protected:
  WebSocketServer server;
};

TEST_F(WebSocketServerTest, StartStop) {
  EXPECT_FALSE(server.IsRunning());
  server.Stop();
  EXPECT_FALSE(server.IsRunning());
}

TEST_F(WebSocketServerTest, SendFrameRejectsNullConnection) {
  EXPECT_FALSE(server.SendText(nullptr, "hello"));
  EXPECT_FALSE(server.SendPing(nullptr));
  EXPECT_FALSE(server.SendPong(nullptr));
}

TEST_F(WebSocketServerTest, CloseAndForceCloseRejectNullConnection) {
  EXPECT_FALSE(server.Close(nullptr));
  server.ForceClose(nullptr);
}

TEST_F(WebSocketServerTest, BroadcastDoesNotCrashWithoutConnections) {
  EXPECT_EQ(server.Broadcast(std::vector<uint8_t>{'h', 'i'}), 0);
}

TEST_F(WebSocketServerTest, ConnectionStateHelpers) {
  auto conn = std::make_shared<Connection>(123, "127.0.0.1:5555");
  EXPECT_TRUE(conn->IsConnected());
  EXPECT_EQ(conn->phase(), SessionPhase::kHandshake);
  EXPECT_EQ(conn->connection_id(), 123);
  EXPECT_EQ(conn->remote_address(), "127.0.0.1:5555");

  conn->set_phase(SessionPhase::kWebSocket);
  conn->set_closing(true);
  conn->set_connected(false);

  EXPECT_EQ(conn->phase(), SessionPhase::kWebSocket);
  EXPECT_TRUE(conn->is_closing());
  EXPECT_FALSE(conn->IsConnected());
}

}  // namespace
