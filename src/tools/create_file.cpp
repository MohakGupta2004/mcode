#include "create_file.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ostream>

std::string CreateFile::execute(nlohmann::json &arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }
  if (!arguements.contains("content") || !arguements["content"].is_string()) {
    return "Error: missing required string argument 'content'";
  }
  if (!arguements.contains("extension") || !arguements["extension"].is_string()) {
    return "Error: missing required string argument 'extension'";
  }

  std::string path = arguements["path"].get<std::string>();
  std::string extension = arguements["extension"].get<std::string>();
  std::string content = arguements["content"].get<std::string>();

  std::ofstream output(path + extension);
  if (!output.is_open()) {
    return "Error: Could not create or open the file!"; 
  }

  output<<content<<std::endl;
  output.clear();
  return path+extension+" file has been created";
}
