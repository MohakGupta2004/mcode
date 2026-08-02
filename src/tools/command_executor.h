#pragma once

#include "tool.h"
#include <string>

class CommandExecutor : public Tool {
  std::string getName() const override { return "command_executor"; }
  std::string getDescription() const override {
    return "Execute a command in the user's Linux/macOS shell (bash/zsh/sh) and return its "
           "stdout, stderr, and exit code. Use this when a task needs something none of the "
           "other tools cover: running a build, installing a package, running tests, checking "
           "git status, listing processes, running a compiler, or any other shell-level "
           "operation. Do not use this for reading, writing, editing, creating, or deleting "
           "files or directories - dedicated tools exist for those and should be preferred. "
           "The command always requires the user's explicit y/n confirmation before it runs, "
           "so only call this when running a command is actually necessary to make progress.";
  }

  nlohmann::json getParameters() const override {
    return {
      {"type", "object"},
      {"properties", {
        {"command", {
          {"type", "string"},
          {"description", "A single shell command to run exactly as-is in bash/zsh/sh, e.g. "
                          "'npm install', 'git status', 'ls -la', 'make build'."}
        }}
      }},
      {"required", {"command"}}
    };
  }

  std::string execute(nlohmann::json &arguements) override;
};
