// Copyright (C) 2025
// Licensed under the MIT License

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <darwincore/network/server.h>
#include "darwincore/websocket/frame_parser.h"
#include "darwincore/websocket/frame_builder.h"
#include "darwincore/websocket/handshake_handler.h"
#include "darwincore/websocket/jsonrpc/request_handler.h"

namespace darwincore {

namespace websocket {

/**
 * @brief WebSocket + JSON-RPC 服务器
 * 
 * 基于 DarwinCore::Network::Server 实现，提供 WebSocket 协议支持
 * 和 JSON-RPC 2.0 方法调用功能。
 * 
 * 特性：
 * - 自动处理 WebSocket 握手
 * - 支持 Ping/Pong 心跳
 * - JSON-RPC 方法注册和路由
 * - 服务端主动推送（通知）
 * - 多客户端连接管理
 */
class JsonRpcServer {
public:
    /**
     * @brief 连接状态
     */
    struct ConnectionState {
        bool handshake_completed = false;  ///< 是否完成握手
        std::vector<uint8_t> recv_buffer;  ///< 接收缓冲区
        std::unique_ptr<FrameParser> parser;  ///< 帧解析器
        size_t processed_offset = 0;  ///< 已处理偏移量（优化缓冲区管理）
    };

    /**
     * @brief 构造函数
     */
    JsonRpcServer();
    
    /**
     * @brief 析构函数
     */
    ~JsonRpcServer();

    // 禁止拷贝和移动
    JsonRpcServer(const JsonRpcServer&) = delete;
    JsonRpcServer& operator=(const JsonRpcServer&) = delete;
    JsonRpcServer(JsonRpcServer&&) = delete;
    JsonRpcServer& operator=(JsonRpcServer&&) = delete;

    // ==================== 服务器控制 ====================

    /**
     * @brief 启动 IPv4 服务器
     * @param host 监听地址（如 "0.0.0.0"）
     * @param port 监听端口
     * @return 成功返回 true
     */
    bool Start(const std::string& host, uint16_t port);
    
    /**
     * @brief 停止服务器
     */
    void Stop();
    
    /**
     * @brief 服务器是否正在运行
     */
    bool IsRunning() const;

    // ==================== JSON-RPC 方法注册 ====================

    /**
     * @brief 注册 RPC 方法
     * @param method 方法名
     * @param handler 处理函数
     */
    void RegisterMethod(const std::string& method, jsonrpc::MethodHandler handler);

    // ==================== 服务端推送 ====================

    /**
     * @brief 向指定客户端发送 JSON-RPC 通知
     * @param connection_id 连接 ID
     * @param method 通知方法名
     * @param params 参数
     * @return 成功返回 true
     */
    bool SendNotification(uint64_t connection_id,
                          const std::string& method,
                          const nlohmann::json& params);
    
    /**
     * @brief 向所有客户端广播 JSON-RPC 通知
     * @param method 通知方法名
     * @param params 参数
     */
    void BroadcastNotification(const std::string& method,
                               const nlohmann::json& params);

    // ==================== 连接管理 ====================

    /**
     * @brief 获取当前连接数
     */
    size_t GetConnectionCount() const;
    
    /**
     * @brief 关闭指定连接
     * @param connection_id 连接 ID
     * @param code WebSocket 关闭码（默认 1000 正常关闭）
     * @param reason 关闭原因
     */
    void CloseConnection(uint64_t connection_id,
                         uint16_t code = 1000,
                         const std::string& reason = "");

    // ==================== 事件回调 ====================

    /**
     * @brief 设置客户端连接回调
     * @param callback 回调函数
     */
    void SetOnClientConnected(
        std::function<void(uint64_t connection_id,
                          const network::ConnectionInformation&)> callback);
    
    /**
     * @brief 设置客户端断开回调
     * @param callback 回调函数
     */
    void SetOnClientDisconnected(
        std::function<void(uint64_t connection_id)> callback);
    
    /**
     * @brief 设置连接错误回调
     * @param callback 回调函数
     */
    void SetOnError(
        std::function<void(uint64_t connection_id,
                          const std::string& error)> callback);

private:
    /// DarwinCore Network 服务器（指针，必须在堆上创建）
    std::unique_ptr<network::Server> network_server_;
    
    /// JSON-RPC 请求处理器
    std::unique_ptr<jsonrpc::RequestHandler> rpc_handler_;
    
    /// 连接状态管理
    mutable std::mutex connections_mutex_;
    std::unordered_map<uint64_t, ConnectionState> connections_;
    
    /// 用户回调
    std::function<void(uint64_t, const network::ConnectionInformation&)> on_connected_;
    std::function<void(uint64_t)> on_disconnected_;
    std::function<void(uint64_t, const std::string&)> on_error_;
    
    /// 服务器状态
    bool is_running_ = false;

    // ==================== 内部事件处理 ====================

    /**
     * @brief 处理新连接（来自 DarwinCore Network）
     */
    void OnNetworkConnected(const network::ConnectionInformation& info);
    
    /**
     * @brief 处理接收到的数据（来自 DarwinCore Network）
     */
    void OnNetworkMessage(uint64_t connection_id,
                         const std::vector<uint8_t>& data);
    
    /**
     * @brief 处理连接断开（来自 DarwinCore Network）
     */
    void OnNetworkDisconnected(uint64_t connection_id);
    
    /**
     * @brief 处理网络错误（来自 DarwinCore Network）
     */
    void OnNetworkError(uint64_t connection_id,
                        network::NetworkError error,
                        const std::string& message);

    // ==================== WebSocket 协议处理 ====================

    /**
     * @brief 处理 WebSocket 握手
     * @return 握手成功返回 true
     */
    bool HandleHandshake(uint64_t connection_id,
                        const std::string& data);
    
    /**
     * @brief 处理 WebSocket 帧
     */
    void HandleWebSocketFrame(uint64_t connection_id,
                             const Frame& frame);
    
    /**
     * @brief 发送 WebSocket 帧
     */
    bool SendWebSocketFrame(uint64_t connection_id,
                            const std::vector<uint8_t>& payload,
                            OpCode opcode);
    
    // ==================== JSON-RPC 处理 ====================

    /**
     * @brief 处理 JSON-RPC 请求
     */
    void HandleJsonRpcRequest(uint64_t connection_id,
                              const std::string& request);
};

}  // namespace websocket
}  // namespace darwincore
