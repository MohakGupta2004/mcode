#include "tool.h"
#include "tool_manager.h"
#include <exception>

void ToolManager::registerTool(std::shared_ptr<Tool> tool) {
  tools[tool->getName()] = tool;
}


std::string ToolManager::execute(nlohmann::json& args, std::string tool_name){
  auto it = tools.find(tool_name);
  if (it == tools.end()) {
    return "Error: unknown tool '" + tool_name + "'";
  }
  // Tools run on model-supplied, untrusted arguments. A malformed/missing
  // field must come back as an error string the model can react to, not an
  // uncaught exception that takes down the whole process and loses the
  // in-memory conversation.
  try {
    return it->second->execute(args);
  } catch (const std::exception& e) {
    return std::string("Error: tool '") + tool_name + "' failed: " + e.what();
  } catch (...) {
    return "Error: tool '" + tool_name + "' failed with an unknown error";
  }
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
