// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/session.h"

namespace {

using namespace darwincore::websocket;

}  // namespace

TEST(WebSocketSession, DefaultState) {
  WebSocketSession session;

  EXPECT_EQ(session.phase(), SessionPhase::kHandshake);
  EXPECT_FALSE(session.is_closing());
  EXPECT_EQ(session.processed_offset(), 0);
  EXPECT_NE(session.parser(), nullptr);
}

TEST(WebSocketSession, PhaseTransition) {
  WebSocketSession session;

  EXPECT_EQ(session.phase(), SessionPhase::kHandshake);

  session.set_phase(SessionPhase::kWebSocket);
  EXPECT_EQ(session.phase(), SessionPhase::kWebSocket);

  session.set_phase(SessionPhase::kClosing);
  EXPECT_EQ(session.phase(), SessionPhase::kClosing);
  EXPECT_FALSE(session.is_closing());  // is_closing is a separate flag

  session.set_closing(true);  // Need to set explicitly
  EXPECT_TRUE(session.is_closing());

  session.set_phase(SessionPhase::kClosed);
  EXPECT_EQ(session.phase(), SessionPhase::kClosed);
}

TEST(WebSocketSession, ProcessedOffset) {
  WebSocketSession session;

  EXPECT_EQ(session.processed_offset(), 0);

  session.set_processed_offset(100);
  EXPECT_EQ(session.processed_offset(), 100);

  session.set_processed_offset(200);
  EXPECT_EQ(session.processed_offset(), 200);
}

TEST(WebSocketSession, Reset) {
  WebSocketSession session;

  session.set_phase(SessionPhase::kWebSocket);
  session.set_closing(true);
  session.set_processed_offset(500);

  session.Reset();

  EXPECT_EQ(session.phase(), SessionPhase::kHandshake);
  EXPECT_FALSE(session.is_closing());
  EXPECT_EQ(session.processed_offset(), 0);
}

TEST(WebSocketSession, ParserAccessible) {
  WebSocketSession session;

  auto* parser1 = session.parser();
  auto* parser2 = session.parser();

  EXPECT_EQ(parser1, parser2);
  EXPECT_NE(parser1, nullptr);
}
