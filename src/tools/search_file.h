#pragma once

#include "tool.h"
#include <string>

class SearchFile : public Tool {
  std::string getName() const override { return "search_file"; }
  std::string getDescription() const override {
    return "Search recursively for files by name using shell-style glob patterns. Use path as the directory to search and default it to '.' if omitted. Convert the user's file search request into an appropriate glob pattern: preserve exact names, use '*' only when the user implies a wildcard or partial match, and do not convert glob patterns into regex syntax. Respect user constraints like prefixes, suffixes, extensions, or contains searches. Perform case-insensitive matching and return all matching file paths.";
  } 

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"path", {
          {"type", "string"},
          {"description", "Directory to search in"}
        }},
        {"pattern", {
          {"type", "string"},
          {"description", "Regex pattern to search for"}
        }}
      }},
      {"required", {"path", "pattern"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
