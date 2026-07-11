#pragma once

#include "tool.h"
#include <string>

class CreateDirectory : public Tool {
  std::string getName() const override { return "create_directory"; }
  std::string getDescription() const override {
    return "Create a directory with a appropriate content with particular naming convention.";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"path", {
          {"type", "string"},
          {"description", "Absolute or relative path of where to create the directory."}
        }},
      }},
      {"required", {"path"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
