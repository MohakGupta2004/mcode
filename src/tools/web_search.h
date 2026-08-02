#pragma once

#include "tool.h"
#include <string>

class WebSearch : public Tool {
  std::string getName() const override { return "web_search"; }
  std::string getDescription() const override {
    return "Fetch the contents of a URL over HTTP(S) and return the response body "
           "(truncated if large). This performs a direct GET request - it is not a "
           "search engine, so it only works when you already have a specific URL "
           "to retrieve.";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"url", {
          {"type", "string"},
          {"description", "The full URL to fetch, including scheme (http:// or https://)."}
        }}
      }},
      {"required", {"url"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
