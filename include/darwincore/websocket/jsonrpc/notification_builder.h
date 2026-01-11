// Copyright (C) 2025
// Licensed under the MIT License

#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace darwincore {
namespace jsonrpc {

/**
 * @brief JSON-RPC 通知构建器
 * 
 * 负责构建 JSON-RPC 2.0 格式的通知消息。
 * 通知没有 id 字段，不需要响应。
 */
class NotificationBuilder {
public:
    /**
     * @brief 创建 JSON-RPC 通知
     * @param method 方法名
     * @param params 参数（可以是对象或数组）
     * @return JSON 通知字符串
     */
    static std::string Create(const std::string& method,
                               const nlohmann::json& params);
    
    /**
     * @brief 创建不带参数的 JSON-RPC 通知
     * @param method 方法名
     * @return JSON 通知字符串
     */
    static std::string Create(const std::string& method);
};

}  // namespace jsonrpc
}  // namespace darwincore
