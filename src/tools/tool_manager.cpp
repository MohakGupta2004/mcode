#include "tool.h"
#include "tool_manager.h"

void ToolManager::registerTool(std::shared_ptr<Tool> tool) {
  tools[tool->getName()] = tool;
}


std::string ToolManager::execute(nlohmann::json& args, std::string tool_name){
  auto it = tools.find(tool_name);
  if (it == tools.end()) {
    return "Error: unknown tool '" + tool_name + "'";
  }
  return it->second->execute(args);
}

bool ToolManager::has(const std::string& tool_name) const {
  return tools.find(tool_name) != tools.end();
}

nlohmann::json ToolManager::getDefinitions() const {
  nlohmann::json defs = nlohmann::json::array();
  for (const auto& [name, tool] : tools) {
    defs.push_back(tool->getDefinition());
  }
  return defs;
}
