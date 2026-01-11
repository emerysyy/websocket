// Copyright (C) 2025
// Licensed under the MIT License

#include "darwincore/websocket/jsonrpc/request_handler.h"

#include <sstream>

namespace darwincore {
namespace jsonrpc {

RequestHandler::RequestHandler() {
    // 注册内置方法（如果需要）
}

void RequestHandler::RegisterMethod(const std::string& method,
                                     MethodHandler handler) {
    methods_[method] = std::move(handler);
}

std::string RequestHandler::HandleRequest(const std::string& request_json) {
    nlohmann::json request;
    
    try {
        request = nlohmann::json::parse(request_json);
    } catch (const nlohmann::json::parse_error&) {
        // JSON 解析错误
        nlohmann::json error_response = CreateErrorResponse(
            nlohmann::json::value_t::null,
            kParseError,
            "Parse error"
        );
        return error_response.dump();
    }
    
    // 检查是否是批量请求
    if (request.is_array()) {
        return HandleBatch(request_json);
    }
    
    // 处理单个请求
    auto response = ProcessSingleRequest(request);
    if (response.has_value()) {
        return response->dump();
    }
    
    // 通知类请求没有响应
    return "";
}

std::string RequestHandler::HandleBatch(const std::string& batch_json) {
    nlohmann::json batch;
    
    try {
        batch = nlohmann::json::parse(batch_json);
    } catch (const nlohmann::json::parse_error&) {
        // JSON 解析错误
        nlohmann::json error_response = CreateErrorResponse(
            nlohmann::json::value_t::null,
            kParseError,
            "Parse error"
        );
        return error_response.dump();
    }
    
    if (!batch.is_array()) {
        nlohmann::json error_response = CreateErrorResponse(
            nlohmann::json::value_t::null,
            kInvalidRequest,
            "Batch request must be an array"
        );
        return error_response.dump();
    }
    
    std::vector<nlohmann::json> responses;
    
    for (const auto& request : batch) {
        auto response = ProcessSingleRequest(request);
        if (response.has_value()) {
            responses.push_back(*response);
        }
    }
    
    // 如果全部是通知，返回空字符串
    if (responses.empty()) {
        return "";
    }
    
    return nlohmann::json(responses).dump();
}

std::vector<std::string> RequestHandler::RegisteredMethods() const {
    std::vector<std::string> result;
    result.reserve(methods_.size());
    
    for (const auto& [method, _] : methods_) {
        result.push_back(method);
    }
    
    return result;
}

nlohmann::json RequestHandler::CreateErrorResponse(const nlohmann::json& id,
                                                    int code,
                                                    const std::string& message) {
    nlohmann::json error;
    error["code"] = code;
    error["message"] = message;
    
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"] = error;
    
    if (!id.is_null() && id != nlohmann::json::value_t::null) {
        response["id"] = id;
    }
    
    return response;
}

nlohmann::json RequestHandler::CreateSuccessResponse(const nlohmann::json& id,
                                                      const nlohmann::json& result) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    return response;
}

bool RequestHandler::ValidateRequest(const nlohmann::json& request) const {
    // 检查是否是对象
    if (!request.is_object()) {
        return false;
    }
    
    // 检查 jsonrpc 版本
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return false;
    }
    
    // 检查 method 字段
    if (!request.contains("method") || !request["method"].is_string()) {
        return false;
    }
    
    return true;
}

std::optional<nlohmann::json> RequestHandler::ProcessSingleRequest(
    const nlohmann::json& request) {
    // 验证请求格式
    if (!ValidateRequest(request)) {
        nlohmann::json id;
        if (request.contains("id")) {
            id = request["id"];
        }
        return CreateErrorResponse(id, kInvalidRequest, "Invalid Request");
    }
    
    std::string method_name = request["method"];
    
    // 获取 params（可选）
    nlohmann::json params;
    if (request.contains("params")) {
        params = request["params"];
    }
    
    // 获取 id（通知可能没有 id）
    nlohmann::json id;
    if (request.contains("id")) {
        id = request["id"];
    }
    
    // 检查是否是通知（没有 id）
    bool is_notification = !request.contains("id");
    
    // 查找方法
    auto it = methods_.find(method_name);
    if (it == methods_.end()) {
        if (is_notification) {
            return std::nullopt;  // 通知不返回错误
        }
        return CreateErrorResponse(id, kMethodNotFound, "Method not found");
    }
    
    // 调用方法
    try {
        nlohmann::json result = it->second(params);
        
        if (is_notification) {
            return std::nullopt;  // 通知不返回响应
        }
        
        return CreateSuccessResponse(id, result);
    } catch (const std::exception& e) {
        if (is_notification) {
            return std::nullopt;
        }
        return CreateErrorResponse(id, kInternalError,
                                   std::string("Internal error: ") + e.what());
    }
}

}  // namespace jsonrpc
}  // namespace darwincore
