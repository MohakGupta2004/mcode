#include "delete_directory.h"
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <filesystem>
namespace fs = std::filesystem;

std::string DeleteDirectory::execute(nlohmann::json &arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }

  std::string path = arguements["path"].get<std::string>();

  std::cout << "Delete directory '" << path << "' and all its contents? (y/n): ";
  char answer = 'n';
  if (!(std::cin >> answer)) {
    std::cin.clear();
  }
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (answer != 'y' && answer != 'Y') {
    return "Deletion cancelled by user for directory: " + path;
  }

  try {
    if (!fs::exists(path)) {
      return "Error: directory does not exist at path: " + path;
    }
    std::uintmax_t removed = fs::remove_all(path);
    if (removed == 0) {
      return "Error: Could not delete directory at path: " + path;
    }
  } catch (const fs::filesystem_error& e) {
    return std::string("Error: could not delete directory '") + path + "': " + e.what();
  }

  return path + " directory has been deleted";
}
