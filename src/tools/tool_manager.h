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
};
