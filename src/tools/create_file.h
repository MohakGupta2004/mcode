#pragma once

#include "tool.h"
#include <string>

class CreateFile : public Tool {
  std::string getName() const override { return "create_file"; }
  std::string getDescription() const override {
    return "Create a file with a appropriate content with particular naming convention with a proper extension of the file.";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"path", {
          {"type", "string"},
          {"description", "Absolute or relative path of the file to read. If nothing mention then the current directory."}
        }},
        {"content", {
          {"type", "string"},
          {"description", "Content of the file which should be written or overwritten or added. If not mentioned then empty file."}
        }},
        {"extension", {
          {"type", "string"},
          {"description", "Extension of the file which should be added after the file. If not mentioned then .md file."}
        }}
      }},
      {"required", {"path", "content", "extension"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
