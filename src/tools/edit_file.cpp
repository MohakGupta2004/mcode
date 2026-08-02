#include "edit_file.h"
#include <fstream>
#include <sstream>
#include <ostream>
#include <string>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <chrono>
#include <nlohmann/json.hpp>

namespace {
// Count non-overlapping occurrences of `needle` in `haystack`.
size_t countOccurrences(const std::string& haystack, const std::string& needle) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.length();
  }
  return count;
}
} // namespace

std::string EditFile::execute(nlohmann::json &arguements) {
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }
  if (!arguements.contains("oldString") || !arguements["oldString"].is_string()) {
    return "Error: missing required string argument 'oldString'";
  }
  if (!arguements.contains("newString") || !arguements["newString"].is_string()) {
    return "Error: missing required string argument 'newString'";
  }

  std::string path = arguements["path"].get<std::string>();
  std::string oldString = arguements["oldString"].get<std::string>();
  std::string newString = arguements["newString"].get<std::string>();
  bool replaceAll = false;
  if (arguements.contains("replaceAll")) {
    replaceAll = arguements["replaceAll"];
  }

  if (oldString.empty()) {
    return "Error: oldString must not be empty.";
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return "Error: Could not open file at path: " + path;
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                      (std::istreambuf_iterator<char>()));
  file.close();

  size_t pos = content.find(oldString);
  if (pos == std::string::npos) {
    return "Error: Old string not found in the file.";
  }

  if (!replaceAll) {
    size_t occurrences = countOccurrences(content, oldString);
    if (occurrences > 1) {
      return "Error: oldString is not unique in the file (" +
             std::to_string(occurrences) +
             " occurrences). Provide more surrounding context to make it "
             "unique, or pass replaceAll=true.";
    }
  }

  if (replaceAll) {
    while (pos != std::string::npos) {
      content.replace(pos, oldString.length(), newString);
      pos = content.find(oldString, pos + newString.length());
    }
  } else {
    content.replace(pos, oldString.length(), newString);
  }

  // Unique-per-process temp file names so concurrent runs/tools never collide.
  std::filesystem::path tmpDir = std::filesystem::temp_directory_path();
  auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  std::string suffix = std::to_string(::getpid()) + "_" + std::to_string(now);
  std::filesystem::path originalPath = tmpDir / ("mcode_original_" + suffix + ".txt");
  std::filesystem::path modifiedPath = tmpDir / ("mcode_modified_" + suffix + ".txt");

  std::ifstream origForDiff(path);
  std::string originalContent((std::istreambuf_iterator<char>(origForDiff)),
                              (std::istreambuf_iterator<char>()));
  origForDiff.close();

  std::ofstream originalFile(originalPath);
  originalFile << originalContent;
  originalFile.close();

  std::ofstream modifiedFile(modifiedPath);
  modifiedFile << content;
  modifiedFile.close();

  // git's raw header names the two scratch temp files, not the real path the
  // user cares about - drop "diff --git", "index", "---" and "+++" (lines
  // 1-4) and print our own header instead so nothing under the OS temp dir
  // ever reaches the terminal.
  std::cout << "\x1b[1m\x1b[36m" << path << "\x1b[0m\n";
  auto patch = std::string("git --no-pager diff --no-index --color=always --no-prefix -- ") +
               originalPath.string() + " " + modifiedPath.string() + " | sed '1,4d'";
  std::system(patch.c_str());
  std::filesystem::remove(originalPath);
  std::filesystem::remove(modifiedPath);

  std::ofstream outFile(path);
  if (!outFile.is_open()) {
    return "Error: Could not open file for writing at path: " + path;
  }
  outFile << content;
  outFile.close();

  return "Successfully edited the file.";
}