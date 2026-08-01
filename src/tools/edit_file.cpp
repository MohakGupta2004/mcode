#include "edit_file.h"
#include <fstream>
#include <sstream>
#include <ostream>
#include <string>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
std::string EditFile::execute(nlohmann::json &arguements) {
  std::string path = arguements["path"];
  std::string oldString = arguements["oldString"];
  std::string newString = arguements["newString"];
  bool replaceAll = false;
  if (arguements.contains("replaceAll")) {
    replaceAll = arguements["replaceAll"];
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return "Error: Could not open file at path: " + path;
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                      (std::istreambuf_iterator<char>()));
  file.close();
  std::ofstream originalFile("/tmp/original_file.txt");
  originalFile << content;
  originalFile.close();
  size_t pos = content.find(oldString);
  if (pos == std::string::npos) {
    return "Error: Old string not found in the file.";
  }

  if (replaceAll) {
    while (pos != std::string::npos) {
      content.replace(pos, oldString.length(), newString);
      pos = content.find(oldString, pos + newString.length());
    }
  } else {
    content.replace(pos, oldString.length(), newString);
  }


  std::ofstream modifiedFile("/tmp/modified_file.txt");
  modifiedFile << content;
  modifiedFile.close();

  auto patch = std::string("git --no-pager diff --no-index --color=always --no-prefix -- /tmp/original_file.txt /tmp/modified_file.txt | sed '1,2d'");
  std::system(patch.c_str());
  auto removeOriginal = std::string("rm /tmp/original_file.txt");
  std::system(removeOriginal.c_str());
  auto removeModified = std::string("rm /tmp/modified_file.txt");
  std::system(removeModified.c_str());

  std::ofstream outFile(path);
  if (!outFile.is_open()) {
    return "Error: Could not open file for writing at path: " + path;
  }
  outFile << content;
  outFile.close();

  return "Successfully edited the file.";
}