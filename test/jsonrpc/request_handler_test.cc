// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/jsonrpc/request_handler.h"

namespace {

using namespace darwincore::jsonrpc;
using nlohmann::json;

}  // namespace

TEST(RequestHandler, RegisterMethod) {
  RequestHandler handler;
  handler.RegisterMethod("test", [](const json& params) { return json(true); });

  auto methods = handler.RegisteredMethods();
  EXPECT_EQ(methods.size(), 1);
  EXPECT_EQ(methods[0], "test");
}

TEST(RequestHandler, HandleValidRequest) {
  RequestHandler handler;
  handler.RegisterMethod("echo", [](const json& params) { return params; });

  std::string request = R"({"jsonrpc":"2.0","method":"echo","params":[1,2,3],"id":1})";
  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_EQ(parsed["jsonrpc"], "2.0");
  EXPECT_EQ(parsed["id"], 1);
  EXPECT_EQ(parsed["result"], json({1, 2, 3}));
}

TEST(RequestHandler, HandleNotification) {
  RequestHandler handler;
  handler.RegisterMethod("notify", [](const json& params) { return json(nullptr); });

  std::string request = R"({"jsonrpc":"2.0","method":"notify","params":{"key":"value"}})";
  std::string response = handler.HandleRequest(request);

  EXPECT_EQ(response, "");
}

TEST(RequestHandler, MethodNotFound) {
  RequestHandler handler;

  std::string request = R"({"jsonrpc":"2.0","method":"unknown","id":1})";
  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_EQ(parsed["error"]["code"], -32601);
  EXPECT_EQ(parsed["error"]["message"], "Method not found");
}

TEST(RequestHandler, InvalidJson) {
  RequestHandler handler;

  std::string request = "not valid json";
  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_EQ(parsed["error"]["code"], -32700);
}

TEST(RequestHandler, InvalidRequest) {
  RequestHandler handler;

  std::string request = R"({"jsonrpc":"2.0","params":[]})";  // Missing method
  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_EQ(parsed["error"]["code"], -32600);
}

TEST(RequestHandler, InternalError) {
  RequestHandler handler;
  handler.RegisterMethod("error_method", [](const json& params) -> json {
    // 抛出任意异常都会被捕获为 InternalError
    throw std::runtime_error("Internal error");
  });

  std::string request = R"({"jsonrpc":"2.0","method":"error_method","params":[],"id":1})";
  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_EQ(parsed["error"]["code"], -32603);  // Internal error
}

TEST(RequestHandler, BatchRequest) {
  RequestHandler handler;
  handler.RegisterMethod("add", [](const json& params) {
    return params[0].get<int>() + params[1].get<int>();
  });
  handler.RegisterMethod("notify", [](const json&) { return json(nullptr); });

  std::string request = R"([
    {"jsonrpc":"2.0","method":"add","params":[1,2],"id":1},
    {"jsonrpc":"2.0","method":"notify","params":[]},
    {"jsonrpc":"2.0","method":"add","params":[3,4],"id":2}
  ])";

  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_TRUE(parsed.is_array());
  EXPECT_EQ(parsed.size(), 2);  // Notification 不返回

  EXPECT_EQ(parsed[0]["id"], 1);
  EXPECT_EQ(parsed[0]["result"], 3);

  EXPECT_EQ(parsed[1]["id"], 2);
  EXPECT_EQ(parsed[1]["result"], 7);
}

TEST(RequestHandler, WrongJsonrpcVersion) {
  RequestHandler handler;

  std::string request = R"({"jsonrpc":"1.0","method":"test","id":1})";
  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_EQ(parsed["error"]["code"], -32600);
}

TEST(RequestHandler, EmptyBatch) {
  RequestHandler handler;
  // JSON-RPC 2.0: 空批量请求必须返回 Invalid Request 错误
  std::string request = "[]";
  std::string response = handler.HandleRequest(request);

  json parsed = json::parse(response);
  EXPECT_EQ(parsed["error"]["code"], -32600);
}
