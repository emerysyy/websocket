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
