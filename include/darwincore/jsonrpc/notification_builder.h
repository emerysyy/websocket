// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_JSONRPC_NOTIFICATION_BUILDER_H_
#define DARWINCORE_JSONRPC_NOTIFICATION_BUILDER_H_

#include <string>

#include <nlohmann/json.hpp>

namespace darwincore {
namespace jsonrpc {

/**
 * @brief JSON-RPC 通知构建器
 *
 * 构建符合 JSON-RPC 2.0 规范的通知消息。
 * 通知没有 id 字段。
 */
class NotificationBuilder {
 public:
  NotificationBuilder() = delete;
  ~NotificationBuilder() = delete;

  /**
   * @brief 创建带参数的通知
   * @param method 方法名
   * @param params 参数
   * @return JSON 字符串
   */
  static std::string Create(const std::string& method,
                            const nlohmann::json& params);

  /**
   * @brief 创建无参数的通知
   * @param method 方法名
   * @return JSON 字符串
   */
  static std::string Create(const std::string& method);
};

}  // namespace jsonrpc
}  // namespace darwincore

#endif  // DARWINCORE_JSONRPC_NOTIFICATION_BUILDER_H_
