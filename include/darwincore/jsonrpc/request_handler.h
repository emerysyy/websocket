// Copyright (C) 2025
// Licensed under the MIT License

#ifndef DARWINCORE_JSONRPC_REQUEST_HANDLER_H_
#define DARWINCORE_JSONRPC_REQUEST_HANDLER_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace darwincore {
namespace jsonrpc {

using nlohmann::json;
using MethodHandler = std::function<json(const json& params)>;

/**
 * @brief JSON-RPC 错误码
 */
struct JsonRpcError {
  static constexpr int32_t kParseError = -32700;
  static constexpr int32_t kInvalidRequest = -32600;
  static constexpr int32_t kMethodNotFound = -32601;
  static constexpr int32_t kInvalidParams = -32602;
  static constexpr int32_t kInternalError = -32603;
};

/**
 * @brief JSON-RPC 请求处理器
 *
 * 处理 JSON-RPC 2.0 请求、通知和批量请求。
 */
class RequestHandler {
 public:
  RequestHandler() = default;
  ~RequestHandler() = default;

  /**
   * @brief 注册 JSON-RPC 方法
   * @param method 方法名
   * @param handler 处理函数
   */
  void RegisterMethod(const std::string& method, MethodHandler handler);

  /**
   * @brief 处理 JSON-RPC 请求
   * @param request 原始请求 JSON 字符串
   * @return 响应 JSON 字符串，通知返回空字符串
   */
  std::string HandleRequest(const std::string& request);

  /**
   * @brief 处理批量请求
   * @param requests 请求数组
   * @return 响应数组（通知不返回）
   */
  std::vector<json> HandleBatch(const std::vector<json>& requests);

  /**
   * @brief 获取已注册的方法列表
   * @return 方法名列表
   */
  std::vector<std::string> RegisteredMethods() const;

 private:
  /**
   * @brief 处理单个请求
   * @param request 请求对象
   * @return 响应对象（通知返回 nullopt）
   */
  std::optional<json> HandleSingleRequest(const json& request);

  /**
   * @brief 创建错误响应
   */
  static json CreateErrorResponse(int32_t code, const std::string& message,
                                  const std::optional<json>& id = std::nullopt);

  /**
   * @brief 创建成功响应
   */
  static json CreateSuccessResponse(const json& result,
                                   const json& id);

  std::unordered_map<std::string, MethodHandler> methods_;
};

}  // namespace jsonrpc
}  // namespace darwincore

#endif  // DARWINCORE_JSONRPC_REQUEST_HANDLER_H_
