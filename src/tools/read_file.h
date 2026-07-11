#pragma once

#include "tool.h"
#include <string>

class ReadFile : public Tool {
  std::string getName() const override { return "read_file"; }
  std::string getDescription() const override {
    return "Read the contents of a file from the local filesystem given its path";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"path", {
          {"type", "string"},
          {"description", "Absolute or relative path of the file to read"}
        }}
      }},
      {"required", {"path"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
