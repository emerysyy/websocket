// Copyright (C) 2025
// Licensed under the MIT License

#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace darwincore {
namespace jsonrpc {

/**
 * @brief JSON-RPC 方法处理器类型
 */
using MethodHandler = std::function<nlohmann::json(const nlohmann::json&)>;

/**
 * @brief JSON-RPC 2.0 请求处理器
 * 
 * 负责解析和处理 JSON-RPC 2.0 请求。
 * 支持：
 * - 单一请求处理
 * - 批量请求处理
 * - 通知（无 id）
 * - 错误响应
 */
class RequestHandler {
public:
    /**
     * @brief 默认构造函数
     */
    RequestHandler();
    
    /**
     * @brief 注册 RPC 方法
     * @param method 方法名
     * @param handler 处理函数
     */
    void RegisterMethod(const std::string& method, MethodHandler handler);
    
    /**
     * @brief 处理 JSON-RPC 请求
     * @param request_json JSON 请求字符串
     * @return JSON 响应字符串（通知类请求返回空字符串）
     */
    std::string HandleRequest(const std::string& request_json);
    
    /**
     * @brief 批量处理 JSON-RPC 请求
     * @param batch_json JSON 批量请求字符串
     * @return JSON 批量响应字符串（全部为通知时返回空字符串）
     */
    std::string HandleBatch(const std::string& batch_json);
    
    /**
     * @brief 获取已注册的方法列表
     * @return 方法名列表
     */
    std::vector<std::string> RegisteredMethods() const;

private:
    /**
     * @brief 创建错误响应
     * @param id 请求 ID（可为 null）
     * @param code 错误码
     * @param message 错误消息
     * @return 错误响应 JSON
     */
    nlohmann::json CreateErrorResponse(const nlohmann::json& id,
                                        int code,
                                        const std::string& message);
    
    /**
     * @brief 创建成功响应
     * @param id 请求 ID
     * @param result 结果
     * @return 成功响应 JSON
     */
    nlohmann::json CreateSuccessResponse(const nlohmann::json& id,
                                          const nlohmann::json& result);
    
    /**
     * @brief 验证请求格式
     * @param request 请求 JSON
     * @return 格式正确返回 true
     */
    bool ValidateRequest(const nlohmann::json& request) const;
    
    /**
     * @brief 处理单个请求
     * @param request 请求 JSON
     * @return 响应 JSON（通知返回 null）
     */
    std::optional<nlohmann::json> ProcessSingleRequest(const nlohmann::json& request);
    
    std::unordered_map<std::string, MethodHandler> methods_;
    
    // JSON-RPC 错误码常量
    static constexpr int kParseError = -32700;
    static constexpr int kInvalidRequest = -32600;
    static constexpr int kMethodNotFound = -32601;
    static constexpr int kInvalidParams = -32602;
    static constexpr int kInternalError = -32603;
};

}  // namespace jsonrpc
}  // namespace darwincore
