#pragma once

#include "tool.h"
#include <string>

class EditFile : public Tool {
  std::string getName() const override { return "edit_file"; }
  std::string getDescription() const override {
    return "If 'edit' is specified and a file is specified, edit the contents of a file using old string from the agent and "
           "replace with the new string given its path";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"path", {
          {"type", "string"},
          {"description", "Absolute or relative path of the file to read"}
        }},
        {"oldString", {
          {"type", "string"},
          {"description", "It's the old string that'll going to be replaced"}
        }},
        {"newString", {
          {"type", "string"},
          {"description", "It's the new string that'll replace the old string"}
        }},
        {"replaceAll", {
          {"type", "boolean"},
          {"description",
           "If true, all occurrences of the old string will be replaced with "
           "the new string. If false, only the first occurrence will be "
           "replaced"}
        }}
      }},
      {"required", {"path", "oldString", "newString"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
 // namespace tools
