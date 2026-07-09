#pragma once

#include "tool.h"
#include <string>

class ReadFile : public Tool {
  std::string getName() const override { return "read_file"; }
  std::string getDescription() const override {
    return "This tool is useful when the user request for a file read";
  }

  std::string execute(nlohmann::json &arguements) override;
};
