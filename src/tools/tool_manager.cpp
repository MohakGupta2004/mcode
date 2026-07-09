#include "tool.h"
#include "tool_manager.h"

void ToolManager::registerTool(std::shared_ptr<Tool> tool) {
  tools[tool->getName()] = tool;
}


std::string ToolManager::execute(nlohmann::json& args, std::string tool_name){
  std::string result = tools[tool_name]->execute(args);
  return result;
}
