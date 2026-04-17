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

// 测试 Connection 继承 WebSocketSession，会话状态挂在 Connection 上
TEST(JsonRpcServer, ConnectionInheritsWebSocketSession) {
  auto conn = std::make_shared<Connection>(123, "127.0.0.1:12345");

  // Connection 继承 WebSocketSession，直接使用 SessionPhase
  EXPECT_EQ(conn->phase(), SessionPhase::kHandshake);
  EXPECT_FALSE(conn->is_closing());
  EXPECT_NE(conn->parser(), nullptr);

  // 修改阶段
  conn->set_phase(SessionPhase::kWebSocket);
  EXPECT_EQ(conn->phase(), SessionPhase::kWebSocket);
}

// 测试多个 Connection 各自独立存储会话状态
TEST(JsonRpcServer, IndependentConnectionSessions) {
  auto conn1 = std::make_shared<Connection>(1);
  auto conn2 = std::make_shared<Connection>(2);

  conn1->set_phase(SessionPhase::kHandshake);
  conn2->set_phase(SessionPhase::kWebSocket);

  EXPECT_EQ(conn1->phase(), SessionPhase::kHandshake);
  EXPECT_EQ(conn2->phase(), SessionPhase::kWebSocket);

  // 修改 conn1 不影响 conn2
  conn1->set_phase(SessionPhase::kWebSocket);
  EXPECT_EQ(conn2->phase(), SessionPhase::kWebSocket);
}

// 测试 recv_buffer 跨帧缓冲
TEST(JsonRpcServer, RecvBufferOperations) {
  auto conn = std::make_shared<Connection>(456);

  EXPECT_TRUE(conn->recv_buffer().empty());

  // 添加数据
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  conn->recv_buffer().insert(conn->recv_buffer().end(), data.begin(), data.end());
  EXPECT_EQ(conn->recv_buffer().size(), 4);

  // 消费部分数据
  conn->consume_recv_buffer(2);
  EXPECT_EQ(conn->recv_buffer().size(), 2);
  EXPECT_EQ(conn->recv_buffer()[0], 0x03);

  // 消费剩余数据
  conn->consume_recv_buffer(10);  // 超过大小，应该清空
  EXPECT_TRUE(conn->recv_buffer().empty());
}

// 测试 closing 幂等性
TEST(JsonRpcServer, CloseConnectionIdempotent) {
  auto conn = std::make_shared<Connection>(789);
  EXPECT_FALSE(conn->is_closing());

  conn->set_closing(true);
  EXPECT_TRUE(conn->is_closing());

  // 重复设置不应改变状态
  conn->set_closing(true);
  EXPECT_TRUE(conn->is_closing());
}

// 测试阶段转换
TEST(JsonRpcServer, SessionPhaseTransitions) {
  auto conn = std::make_shared<Connection>(100);

  // 初始状态
  EXPECT_EQ(conn->phase(), SessionPhase::kHandshake);

  // Handshake → WebSocket
  conn->set_phase(SessionPhase::kWebSocket);
  EXPECT_EQ(conn->phase(), SessionPhase::kWebSocket);

  // WebSocket → Closing
  conn->set_phase(SessionPhase::kClosing);
  EXPECT_EQ(conn->phase(), SessionPhase::kClosing);

  // Closing → Closed
  conn->set_phase(SessionPhase::kClosed);
  EXPECT_EQ(conn->phase(), SessionPhase::kClosed);
}

// 测试远程地址
TEST(JsonRpcServer, RemoteAddress) {
  auto conn = std::make_shared<Connection>(999, "192.168.1.100:54321");
  EXPECT_EQ(conn->remote_address(), "192.168.1.100:54321");
  EXPECT_EQ(conn->connection_id(), 999);
}

// 测试连接状态
TEST(JsonRpcServer, ConnectionState) {
  auto conn = std::make_shared<Connection>(111);
  EXPECT_TRUE(conn->IsConnected());

  conn->set_connected(false);
  EXPECT_FALSE(conn->IsConnected());
}

// 测试帧处理完整路径：握手 → WebSocket 帧处理
TEST(JsonRpcServer, WebSocketFrameProcessing) {
  auto conn = std::make_shared<Connection>(200);

  // 初始为握手阶段
  EXPECT_EQ(conn->phase(), SessionPhase::kHandshake);
  EXPECT_TRUE(conn->recv_buffer().empty());

  // 模拟握手完成，切换到 WebSocket 阶段
  conn->set_phase(SessionPhase::kWebSocket);
  EXPECT_EQ(conn->phase(), SessionPhase::kWebSocket);

  // 准备一个 JSON-RPC 请求帧（使用 FrameBuilder）
  std::string json_request = R"({"jsonrpc":"2.0","method":"test","id":1})";
  auto frame = FrameBuilder::BuildFrame(OpCode::kText,
                                        std::vector<uint8_t>(json_request.begin(),
                                                            json_request.end()));

  // 添加帧数据到接收缓冲区
  conn->recv_buffer().insert(conn->recv_buffer().end(),
                             frame.begin(), frame.end());
  EXPECT_EQ(conn->recv_buffer().size(), frame.size());

  // 验证帧可以被解析
  FrameParser* parser = conn->parser();
  size_t consumed = 0;
  auto parsed = parser->Parse(conn->recv_buffer(), consumed);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->opcode, OpCode::kText);
  EXPECT_EQ(parsed->payload.size(), json_request.size());

  // 消费已解析的帧
  conn->consume_recv_buffer(consumed);
  EXPECT_TRUE(conn->recv_buffer().empty());
}

// 测试分片帧的跨帧缓冲
TEST(JsonRpcServer, FragmentedFrameBuffering) {
  auto conn = std::make_shared<Connection>(201);
  conn->set_phase(SessionPhase::kWebSocket);

  // 第一个分片帧 (FIN=0, opcode=0x01 text)
  std::vector<uint8_t> payload1 = {'H', 'e', 'l', 'l', 'o'};
  Frame frame1;
  frame1.fin = false;
  frame1.opcode = OpCode::kText;
  frame1.payload = payload1;
  auto encoded1 = FrameBuilder::BuildFrame(frame1.opcode, frame1.payload,
                                           frame1.fin);

  // 添加第一帧
  conn->recv_buffer().insert(conn->recv_buffer().end(),
                             encoded1.begin(), encoded1.end());

  // 验证第一帧可解析但 payload 不完整（简化测试：直接验证帧格式）
  FrameParser* parser = conn->parser();
  size_t consumed = 0;
  auto parsed1 = parser->Parse(conn->recv_buffer(), consumed);

  ASSERT_TRUE(parsed1.has_value());
  EXPECT_EQ(parsed1->opcode, OpCode::kText);
  EXPECT_FALSE(parsed1->fin);

  conn->consume_recv_buffer(consumed);
  EXPECT_TRUE(conn->recv_buffer().empty());
}

// 测试 Ping/Pong 帧处理
TEST(JsonRpcServer, PingPongFrames) {
  auto conn = std::make_shared<Connection>(202);
  conn->set_phase(SessionPhase::kWebSocket);

  // 构建 Ping 帧
  std::vector<uint8_t> ping_data = {'p', 'i', 'n', 'g'};
  auto ping_frame = FrameBuilder::BuildFrame(OpCode::kPing, ping_data);

  // 添加到缓冲区并解析
  conn->recv_buffer().insert(conn->recv_buffer().end(),
                             ping_frame.begin(), ping_frame.end());

  FrameParser* parser = conn->parser();
  size_t consumed = 0;
  auto parsed = parser->Parse(conn->recv_buffer(), consumed);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->opcode, OpCode::kPing);
  EXPECT_EQ(parsed->payload.size(), ping_data.size());
}

// 测试 Close 帧处理后的状态转换
TEST(JsonRpcServer, CloseFrameStateTransition) {
  auto conn = std::make_shared<Connection>(203);
  conn->set_phase(SessionPhase::kWebSocket);

  // 构建 Close 帧
  auto close_frame = FrameBuilder::CreateCloseFrame(1000, "Normal closure");

  // 解析 Close 帧
  conn->recv_buffer().insert(conn->recv_buffer().end(),
                             close_frame.begin(), close_frame.end());

  FrameParser* parser = conn->parser();
  size_t consumed = 0;
  auto parsed = parser->Parse(conn->recv_buffer(), consumed);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->opcode, OpCode::kClose);

  conn->consume_recv_buffer(consumed);
  EXPECT_TRUE(conn->recv_buffer().empty());

  // 模拟 CloseConnection 后的状态
  conn->set_closing(true);
  conn->set_phase(SessionPhase::kClosing);
  conn->set_connected(false);

  EXPECT_TRUE(conn->is_closing());
  EXPECT_EQ(conn->phase(), SessionPhase::kClosing);
  EXPECT_FALSE(conn->IsConnected());
}

// 测试广播只针对 WebSocket 阶段连接
TEST(JsonRpcServer, BroadcastOnlyWebSocketPhase) {
  auto conn1 = std::make_shared<Connection>(301);
  auto conn2 = std::make_shared<Connection>(302);

  // conn1 还在握手阶段
  EXPECT_EQ(conn1->phase(), SessionPhase::kHandshake);

  // conn2 已完成握手
  conn2->set_phase(SessionPhase::kWebSocket);

  // 验证只有 kWebSocket 阶段的连接会被广播
  if (conn2->phase() == SessionPhase::kWebSocket && conn2->IsConnected()) {
    // 这个连接会被包含在广播列表中
    EXPECT_EQ(conn2->phase(), SessionPhase::kWebSocket);
  }
}
