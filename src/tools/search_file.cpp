#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include "search_file.h"
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>
namespace fs = std::filesystem; 
std::string SearchFile::execute(nlohmann::json& arguements)  {
  if (!arguements.contains("pattern") || !arguements["pattern"].is_string()) {
    return "Error: missing required string argument 'pattern'";
  }
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }

  std::string path = arguements["path"].get<std::string>();
  std::string pattern = arguements["pattern"].get<std::string>();
  std::cout << "Searching in path: " << path << " for pattern: " << pattern << std::endl;
  std::regex regexPattern;
  try {
    regexPattern = std::regex(pattern);
  } catch (const std::regex_error& e) {
    return std::string("Error: invalid regex pattern: ") + e.what();
  }

  std::string result;
  try {
    for(const auto& entry : fs::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        std::ifstream file(entry.path());
        if (!file.is_open()) {
          result += "Error: Could not open file at path: " + entry.path().string() + "\n";
          continue;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            (std::istreambuf_iterator<char>()));
        file.close();

        std::smatch match;
        if (std::regex_search(content, match, regexPattern)) {
          result += "Match found in file: " + entry.path().string() + "\n";
          result += "Matched text: " + match.str() + "\n";
        }
      }
    }
  } catch (const fs::filesystem_error& e) {
    return std::string("Error: could not search path '") + path + "': " + e.what();
  }

  return result;
}

