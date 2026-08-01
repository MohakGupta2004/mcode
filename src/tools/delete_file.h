#pragma once

#include "tool.h"
#include <string>

class DeleteFile : public Tool {
  std::string getName() const override { return "delete_file"; }
  std::string getDescription() const override {
    return "Delete a file at the given path, after asking the user for confirmation.";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"path", {
          {"type", "string"},
          {"description", "Absolute or relative path of the file to delete."}
        }}
      }},
      {"required", {"path"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
