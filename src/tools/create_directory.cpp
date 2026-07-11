#include "create_directory.h"
#include <nlohmann/json.hpp>
#include <filesystem>
namespace fs = std::filesystem;

std::string CreateDirectory::execute(nlohmann::json &arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }

  fs::path path = arguements["path"].get<std::string>();
  if(fs::create_directories(path)) {
    return "Directory has been created";
  } else {
    return "There is a problem creating this directory";
  }
}
