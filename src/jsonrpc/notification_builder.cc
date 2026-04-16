// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/jsonrpc/notification_builder.h"

namespace darwincore {
namespace jsonrpc {

std::string NotificationBuilder::Create(const std::string& method,
                                        const nlohmann::json& params) {
  nlohmann::json notification = {
    {"jsonrpc", "2.0"},
    {"method", method},
    {"params", params}
  };
  return notification.dump();
}

std::string NotificationBuilder::Create(const std::string& method) {
  nlohmann::json notification = {
    {"jsonrpc", "2.0"},
    {"method", method}
  };
  return notification.dump();
}

}  // namespace jsonrpc
}  // namespace darwincore
