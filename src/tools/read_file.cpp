#include <fstream>
#include <sstream>
#include <string>
#include "read_file.h"

std::string ReadFile::execute(nlohmann::json& arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }

  std::string path = arguements["path"].get<std::string>();
  std::ifstream file(path);
  if (!file.is_open()) {
    return "Error: could not open file '" + path + "'";
  }

  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}
