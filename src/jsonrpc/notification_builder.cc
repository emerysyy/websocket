// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/jsonrpc/notification_builder.h"

namespace darwincore {
namespace jsonrpc {

std::string NotificationBuilder::Create(const std::string& method,
                                         const nlohmann::json& params) {
    nlohmann::json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = method;
    
    if (!params.is_null()) {
        notification["params"] = params;
    }
    
    return notification.dump();
}

std::string NotificationBuilder::Create(const std::string& method) {
    return Create(method, nlohmann::json::value_t::null);
}

}  // namespace jsonrpc
}  // namespace darwincore
