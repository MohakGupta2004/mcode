#include "delete_file.h"
#include <cstdio>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>

std::string DeleteFile::execute(nlohmann::json &arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }

  std::string path = arguements["path"].get<std::string>();

  std::cout << "Delete file '" << path << "'? (y/n): ";
  char answer = 'n';
  if (!(std::cin >> answer)) {
    // No input available (e.g. non-interactive stdin) - clear the fail state
    // so subsequent reads elsewhere in the program aren't wedged, and default
    // to declining the destructive action.
    std::cin.clear();
  }
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (answer != 'y' && answer != 'Y') {
    return "Deletion cancelled by user for file: " + path;
  }

  if (std::remove(path.c_str()) != 0) {
    return "Error: Could not delete file at path: " + path;
  }

  return path + " file has been deleted";
}
