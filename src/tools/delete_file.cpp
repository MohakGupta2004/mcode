#include "delete_file.h"
#include <cstdio>
#include <iostream>
#include <nlohmann/json.hpp>

std::string DeleteFile::execute(nlohmann::json &arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }

  std::string path = arguements["path"].get<std::string>();

  std::cout << "Delete file '" << path << "'? (y/n): ";
  char answer = 'n';
  std::cin >> answer;

  if (answer != 'y' && answer != 'Y') {
    return "Deletion cancelled by user for file: " + path;
  }

  if (std::remove(path.c_str()) != 0) {
    return "Error: Could not delete file at path: " + path;
  }

  return path + " file has been deleted";
}
