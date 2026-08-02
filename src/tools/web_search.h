#pragma once

#include "tool.h"
#include <string>

class WebSearch : public Tool {
  std::string getName() const override { return "web_search"; }
  std::string getDescription() const override {
    return "Search the web using a URL";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"url", {
          {"type", "string"},
          {"description", "The URL to search"}
        }}
      }},
      {"required", {"url"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
