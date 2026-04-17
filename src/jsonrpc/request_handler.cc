// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/jsonrpc/request_handler.h"

#include <optional>

namespace darwincore {
namespace jsonrpc {

void RequestHandler::RegisterMethod(const std::string& method,
                                   MethodHandler handler) {
  methods_[method] = std::move(handler);
}

std::string RequestHandler::HandleRequest(const std::string& request) {
  json request_json;

  try {
    request_json = json::parse(request);
  } catch (...) {
    return CreateErrorResponse(JsonRpcError::kParseError, "Parse error").dump();
  }

  // 批量请求
  if (request_json.is_array()) {
    // JSON-RPC 2.0: 空批量请求必须返回 Invalid Request 错误
    if (request_json.empty()) {
      return CreateErrorResponse(JsonRpcError::kInvalidRequest,
                                "Invalid Request: empty batch").dump();
    }
    auto responses = HandleBatch(request_json);
    if (responses.empty()) {
      return "";  // 全部是通知
    }
    return json(responses).dump();
  }

  // 单个请求
  auto response = HandleSingleRequest(request_json);
  if (!response.has_value()) {
    return "";  // 通知不返回
  }
  return response->dump();
}

std::vector<json> RequestHandler::HandleBatch(const std::vector<json>& requests) {
  std::vector<json> responses;

  for (const auto& request : requests) {
    auto response = HandleSingleRequest(request);
    if (response.has_value()) {
      responses.push_back(*response);
    }
  }

  return responses;
}

std::vector<std::string> RequestHandler::RegisteredMethods() const {
  std::vector<std::string> result;
  result.reserve(methods_.size());
  for (const auto& [name, _] : methods_) {
    result.push_back(name);
  }
  return result;
}

std::optional<json> RequestHandler::HandleSingleRequest(const json& request) {
  // 检查是否为有效对象
  if (!request.is_object()) {
    return CreateErrorResponse(JsonRpcError::kInvalidRequest, "Invalid Request",
                               request.contains("id") ? std::optional(request["id"]) : std::nullopt);
  }

  // 检查 jsonrpc 版本
  if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
    return CreateErrorResponse(JsonRpcError::kInvalidRequest, "Invalid Request",
                               request.contains("id") ? std::optional(request["id"]) : std::nullopt);
  }

  // 检查 method
  if (!request.contains("method") || !request["method"].is_string()) {
    return CreateErrorResponse(JsonRpcError::kInvalidRequest, "Invalid Request",
                               request.contains("id") ? std::optional(request["id"]) : std::nullopt);
  }

  std::string method = request["method"];
  bool is_notification = !request.contains("id");

  // 查找方法
  auto it = methods_.find(method);
  if (it == methods_.end()) {
    if (is_notification) {
      return std::nullopt;  // 通知不返回错误
    }
    return CreateErrorResponse(JsonRpcError::kMethodNotFound, "Method not found",
                               request["id"]);
  }

  // 获取参数
  json params = nullptr;
  if (request.contains("params")) {
    params = request["params"];
  }

  // 调用方法
  try {
    json result = it->second(params);
    if (is_notification) {
      return std::nullopt;
    }
    return CreateSuccessResponse(result, request["id"]);
  } catch (const json::exception& e) {
    if (is_notification) {
      return std::nullopt;
    }
    return CreateErrorResponse(JsonRpcError::kInvalidParams, e.what(),
                               request["id"]);
  } catch (...) {
    if (is_notification) {
      return std::nullopt;
    }
    return CreateErrorResponse(JsonRpcError::kInternalError, "Internal error",
                               request["id"]);
  }
}

json RequestHandler::CreateErrorResponse(int32_t code,
                                        const std::string& message,
                                        const std::optional<json>& id) {
  json response = {
    {"jsonrpc", "2.0"},
    {"error", {
      {"code", code},
      {"message", message}
    }}
  };

  if (id.has_value()) {
    response["id"] = *id;
  } else {
    response["id"] = nullptr;
  }

  return response;
}

json RequestHandler::CreateSuccessResponse(const json& result,
                                          const json& id) {
  return {
    {"jsonrpc", "2.0"},
    {"result", result},
    {"id", id}
  };
}

}  // namespace jsonrpc
}  // namespace darwincore
