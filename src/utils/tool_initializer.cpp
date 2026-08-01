#include "tool_initializer.h"
#include "tools/create_directory.h"
#include "tools/create_file.h"
#include "tools/delete_file.h"
#include "tools/edit_file.h"
#include "tools/read_file.h"
#include <memory>

void initializeTools(ToolManager& tool_manager) {
  tool_manager.registerTool(std::make_shared<ReadFile>());
  tool_manager.registerTool(std::make_shared<CreateFile>());
  tool_manager.registerTool(std::make_shared<CreateDirectory>());
  tool_manager.registerTool(std::make_shared<EditFile>());
  tool_manager.registerTool(std::make_shared<DeleteFile>());
}
