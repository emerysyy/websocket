// Copyright (C) 2025
// Licensed under the MIT License

// 简单 WebSocket JSON-RPC 服务器示例
// 启动后等待 JavaScript WebSocket 客户端连接

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <memory>
#include <cstring>

#include "darwincore/network/server.h"
#include "darwincore/websocket/jsonrpc_server.h"

using namespace darwincore::websocket;

JsonRpcServer* g_server_ptr = nullptr;  // 改成原始指针
volatile sig_atomic_t g_running = 1;

// 信号处理（只做最小操作）
void SignalHandler(int signal) {
    // 在信号处理器中只做异步信号安全操作
    g_running = 0;
}

// 清理资源函数
void Cleanup() {
    std::cout << "\nShutting down server..." << std::endl;
    if (g_server_ptr) {
        g_server_ptr->Stop();
    }
    std::cout << "Server stopped gracefully." << std::endl;
}

int main() {
    // 设置信号处理
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    std::cout << "========================================" << std::endl;
    std::cout << "  WebSocket JSON-RPC Server" << std::endl;
    std::cout << "========================================" << std::endl;

    // *** 关键修改：使用栈上对象而不是堆上 unique_ptr ***
    JsonRpcServer server;
    g_server_ptr = &server;

    // ==================== 注册 RPC 方法 ====================

    // Echo 方法 - 返回接收到的参数
    server.RegisterMethod("echo", [](const nlohmann::json& params) {
        std::cout << "[RPC] echo called with: " << params.dump() << std::endl;
        return params;
    });
    
    // 加法方法
    server.RegisterMethod("add", [](const nlohmann::json& params) {
        int a = params["a"].get<int>();
        int b = params["b"].get<int>();
        int result = a + b;
        std::cout << "[RPC] add: " << a << " + " << b << " = " << result << std::endl;
        return nlohmann::json{{"result", result}};
    });
    
    // 乘法方法
    server.RegisterMethod("multiply", [](const nlohmann::json& params) {
        int a = params["a"].get<int>();
        int b = params["b"].get<int>();
        int result = a * b;
        std::cout << "[RPC] multiply: " << a << " * " << b << " = " << result << std::endl;
        return nlohmann::json{{"result", result}};
    });
    
    // 获取服务器时间
    server.RegisterMethod("getTime", [](const nlohmann::json&) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        return nlohmann::json{{"timestamp", time}};
    });
    
    // 获取连接数
    server.RegisterMethod("getConnectionCount", [](const nlohmann::json&) {
        return nlohmann::json{{"count", 0}};  // 需要通过服务器接口获取
    });
    
    // ==================== 设置事件回调 ====================

    server.SetOnClientConnected([&server](uint64_t conn_id,
                                      const darwincore::network::ConnectionInformation& info) {
        std::cout << "\n[+] Client connected: " << conn_id
                  << " from " << info.peer_address << ":" << info.peer_port << std::endl;
        std::cout << "    Total connections: " << server.GetConnectionCount() << std::endl;
    });

    server.SetOnClientDisconnected([&server](uint64_t conn_id) {
        std::cout << "\n[-] Client disconnected: " << conn_id << std::endl;
        std::cout << "    Total connections: " << server.GetConnectionCount() << std::endl;
    });

    server.SetOnError([](uint64_t conn_id, const std::string& error) {
        std::cerr << "\n[!] Connection error " << conn_id << ": " << error << std::endl;
    });
    
    // ==================== 启动服务器 ====================

    const std::string kHost = "127.0.0.1";
    const uint16_t kPort = 9998;
    
    if (!server.Start(kHost, kPort)) {
        std::cerr << "Failed to start server on " << kHost << ":" << kPort << std::endl;
        return 1;
    }
    
    std::cout << "\nServer is listening on ws://" << kHost << ":" << kPort << std::endl;
    std::cout << "\nAvailable RPC methods:" << std::endl;
    std::cout << "  - echo (returns params)" << std::endl;
    std::cout << "  - add ({\"a\": num, \"b\": num})" << std::endl;
    std::cout << "  - multiply ({\"a\": num, \"b\": num})" << std::endl;
    std::cout << "  - getTime ()" << std::endl;
    std::cout << "\nOpen browser to test: examples/client.html" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server.\n" << std::endl;
    
    // 定期广播心跳（每 30 秒）
    std::thread heartbeat_thread([&]() {
        int count = 0;
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (!g_running) break;
            ++count;
            std::cout << "[Heartbeat] Broadcasting to "
                      << server.GetConnectionCount() << " clients..." << std::endl;
            server.BroadcastNotification("heartbeat", {
                {"count", count},
                {"connections", server.GetConnectionCount()}
            });
        }
    });
    heartbeat_thread.detach();
    
    // 保持运行
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 清理资源
    Cleanup();

    return 0;
}
