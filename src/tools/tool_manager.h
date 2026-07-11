#pragma once

#include "tool.h"
#include <memory>
#include <unordered_map>
class ToolManager {
  private:
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools;
  public:
    void registerTool(std::shared_ptr<Tool> tool);
    std::string execute(nlohmann::json& args, std::string tool_name);

    // True when a tool with this name is registered.
    bool has(const std::string& tool_name) const;

    // JSON array of every registered tool's definition, ready to drop into an
    // OpenAI-style `tools` request field. Empty array when nothing is registered.
    nlohmann::json getDefinitions() const;
};
