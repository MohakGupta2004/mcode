#pragma once

#include "tool.h"
#include <string>

class DeleteDirectory : public Tool {
  std::string getName() const override { return "delete_directory"; }
  std::string getDescription() const override {
    return "Delete a directory and its contents at the given path, after asking the user for confirmation.";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"path", {
          {"type", "string"},
          {"description", "Absolute or relative path of the directory to delete."}
        }}
      }},
      {"required", {"path"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
