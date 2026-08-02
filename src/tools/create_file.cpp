#include "create_file.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <ostream>
#include <filesystem>

std::string CreateFile::execute(nlohmann::json &arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }
  if (!arguements.contains("content") || !arguements["content"].is_string()) {
    return "Error: missing required string argument 'content'";
  }


  std::string path = arguements["path"].get<std::string>();
  std::string content = arguements["content"].get<std::string>();

  if (std::filesystem::exists(path)) {
    std::cout << "File '" << path << "' already exists. Overwrite? (y/n): ";
    char answer = 'n';
    if (!(std::cin >> answer)) {
      std::cin.clear();
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (answer != 'y' && answer != 'Y') {
      return "Overwrite cancelled by user for file: " + path;
    }
  }

  std::ofstream output(path);
  if (!output.is_open()) {
    return "Error: Could not create or open the file!"; 
  }

  output<<content<<std::endl;
  output.clear();
  return path+" file has been created";
}
