// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/websocket/jsonrpc/request_handler.h"

using namespace darwincore::jsonrpc;

class RequestHandlerTest : public ::testing::Test {
protected:
    RequestHandler handler_;
    
    void SetUp() override {
        handler_.RegisterMethod("subtract", [](const nlohmann::json& params) {
            int a, b;
            if (params.is_array()) {
                a = params[0];
                b = params[1];
            } else {
                // 对象参数
                a = params["a"];
                b = params["b"];
            }
            return nlohmann::json(a - b);
        });
        
        handler_.RegisterMethod("sum", [](const nlohmann::json& params) {
            int total = 0;
            for (const auto& n : params) {
                total += n.get<int>();
            }
            return nlohmann::json(total);
        });
        
        handler_.RegisterMethod("echo", [](const nlohmann::json& params) {
            return params;
        });
    }
};

TEST_F(RequestHandlerTest, HandlePositionalParams) {
    std::string request = R"({
        "jsonrpc": "2.0",
        "method": "subtract",
        "params": [42, 23],
        "id": 1
    })";
    
    std::string response = handler_.HandleRequest(request);
    auto json = nlohmann::json::parse(response);
    
    EXPECT_EQ(json["jsonrpc"], "2.0");
    EXPECT_EQ(json["result"], 19);
    EXPECT_EQ(json["id"], 1);
}

TEST_F(RequestHandlerTest, HandleObjectParams) {
    std::string request = R"({
        "jsonrpc": "2.0",
        "method": "subtract",
        "params": {"b": 23, "a": 42},
        "id": 1
    })";
    
    std::string response = handler_.HandleRequest(request);
    auto json = nlohmann::json::parse(response);
    
    EXPECT_EQ(json["result"], 19);
}

TEST_F(RequestHandlerTest, HandleArrayParams) {
    std::string request = R"({
        "jsonrpc": "2.0",
        "method": "sum",
        "params": [1, 2, 3, 4, 5],
        "id": 2
    })";
    
    std::string response = handler_.HandleRequest(request);
    auto json = nlohmann::json::parse(response);
    
    EXPECT_EQ(json["result"], 15);
}

TEST_F(RequestHandlerTest, MethodNotFound) {
    std::string request = R"({
        "jsonrpc": "2.0",
        "method": "foobar",
        "id": 1
    })";
    
    std::string response = handler_.HandleRequest(request);
    auto json = nlohmann::json::parse(response);
    
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"]["code"], -32601);
}

TEST_F(RequestHandlerTest, InvalidJson) {
    std::string request = "{\"jsonrpc\": \"2.0\", \"method\": \"foobar\"";
    
    std::string response = handler_.HandleRequest(request);
    auto json = nlohmann::json::parse(response);
    
    EXPECT_EQ(json["error"]["code"], -32700);
}

TEST_F(RequestHandlerTest, Notification) {
    std::string notification = R"({
        "jsonrpc": "2.0",
        "method": "update",
        "params": [1, 2, 3]
    })";
    
    std::string response = handler_.HandleRequest(notification);
    
    // 通知不应该有响应
    EXPECT_TRUE(response.empty());
}

TEST_F(RequestHandlerTest, BatchRequest) {
    std::string batch = R"([
        {"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
        {"jsonrpc": "2.0", "method": "subtract", "params": [42,23], "id": "2"},
        {"jsonrpc": "2.0", "method": "notify_sum", "params": [1,2,3]}
    ])";
    
    std::string response = handler_.HandleBatch(batch);
    auto json = nlohmann::json::parse(response);
    
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 2);  // 通知没有响应
}

TEST_F(RequestHandlerTest, RegisteredMethods) {
    auto methods = handler_.RegisteredMethods();
    
    EXPECT_EQ(methods.size(), 3);
    EXPECT_NE(std::find(methods.begin(), methods.end(), "subtract"), methods.end());
    EXPECT_NE(std::find(methods.begin(), methods.end(), "sum"), methods.end());
    EXPECT_NE(std::find(methods.begin(), methods.end(), "echo"), methods.end());
}
