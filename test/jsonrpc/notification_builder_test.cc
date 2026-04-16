// Copyright (C) 2025
// Licensed under the MIT License

#include <gtest/gtest.h>
#include "darwincore/jsonrpc/notification_builder.h"

namespace {

using namespace darwincore::jsonrpc;
using nlohmann::json;

}  // namespace

TEST(NotificationBuilder, CreateWithParams) {
  std::string result = NotificationBuilder::Create("notify", {{"key", "value"}});

  json parsed = json::parse(result);

  EXPECT_EQ(parsed["jsonrpc"], "2.0");
  EXPECT_EQ(parsed["method"], "notify");
  EXPECT_EQ(parsed["params"]["key"], "value");
  EXPECT_FALSE(parsed.contains("id"));
}

TEST(NotificationBuilder, CreateWithoutParams) {
  std::string result = NotificationBuilder::Create("ping");

  json parsed = json::parse(result);

  EXPECT_EQ(parsed["jsonrpc"], "2.0");
  EXPECT_EQ(parsed["method"], "ping");
  EXPECT_FALSE(parsed.contains("params"));
  EXPECT_FALSE(parsed.contains("id"));
}

TEST(NotificationBuilder, NotificationHasNoId) {
  std::string result = NotificationBuilder::Create("event", {{"data", 42}});

  json parsed = json::parse(result);

  EXPECT_FALSE(parsed.contains("id"));
}

TEST(NotificationBuilder, MultipleParams) {
  std::string result = NotificationBuilder::Create("update",
    {
      {"id", 1},
      {"status", "active"},
      {"count", 10}
    });

  json parsed = json::parse(result);

  EXPECT_EQ(parsed["params"]["id"], 1);
  EXPECT_EQ(parsed["params"]["status"], "active");
  EXPECT_EQ(parsed["params"]["count"], 10);
}
