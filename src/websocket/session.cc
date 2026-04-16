// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/session.h"

namespace darwincore {
namespace websocket {

WebSocketSession::WebSocketSession() = default;

void WebSocketSession::Reset() {
  phase_ = SessionPhase::kHandshake;
  is_closing_ = false;
  processed_offset_ = 0;
}

}  // namespace websocket
}  // namespace darwincore
