#include <string>
#include <regex>
#include "search_file.h"
#include <filesystem>
#include <nlohmann/json.hpp>
namespace fs = std::filesystem;

namespace {
// Translate a shell-style glob ('*', '?') into an ECMAScript regex that
// matches a filename, anchored at both ends, case-insensitively.
std::regex globToRegex(const std::string& glob) {
  std::string out = "^";
  for (char c : glob) {
    switch (c) {
      case '*': out += ".*"; break;
      case '?': out += "."; break;
      case '.': case '+': case '(': case ')': case '[': case ']':
      case '{': case '}': case '^': case '$': case '|': case '\\':
        out += '\\';
        out += c;
        break;
      default:
        out += c;
    }
  }
  out += "$";
  return std::regex(out, std::regex::icase);
}
} // namespace

std::string SearchFile::execute(nlohmann::json& arguements)  {
  if (!arguements.contains("pattern") || !arguements["pattern"].is_string()) {
    return "Error: missing required string argument 'pattern'";
  }
  if (!arguements.contains("path") || !arguements["path"].is_string()) {
    return "Error: missing required string argument 'path'";
  }

  std::string path = arguements["path"].get<std::string>();
  std::string pattern = arguements["pattern"].get<std::string>();

  std::regex nameRegex;
  try {
    nameRegex = globToRegex(pattern);
  } catch (const std::regex_error& e) {
    return std::string("Error: invalid glob pattern: ") + e.what();
  }

  std::string result;
  size_t matches = 0;
  try {
    fs::recursive_directory_iterator it(
        path, fs::directory_options::skip_permission_denied);
    fs::recursive_directory_iterator end;
    for (; it != end; ++it) {
      const auto& entry = *it;
      // Skip noisy/irrelevant directory trees entirely.
      if (entry.is_directory()) {
        const std::string name = entry.path().filename().string();
        if (name == ".git" || name == "node_modules" || name == "build") {
          it.disable_recursion_pending();
        }
        continue;
      }
      if (!entry.is_regular_file()) {
        continue;
      }
      const std::string filename = entry.path().filename().string();
      if (std::regex_match(filename, nameRegex)) {
        result += entry.path().string() + "\n";
        ++matches;
      }
    }
  } catch (const fs::filesystem_error& e) {
    return std::string("Error: could not search path '") + path + "': " + e.what();
  }

  if (matches == 0) {
    return "No files matching '" + pattern + "' found under '" + path + "'.";
  }
  return result;
}

