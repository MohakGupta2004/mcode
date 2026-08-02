#include "command_executor.h"
#include <array>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>

std::string CommandExecutor::execute(nlohmann::json &arguements) {
  if (!arguements.contains("command") || !arguements["command"].is_string()) {
    return "Error: missing required string argument 'command'";
  }

  std::string command = arguements["command"].get<std::string>();

  // Always ask for user approval before running a command. There is no
  // auto-approve mode yet; that's a future toggle, not a default.
  std::cout << "Run command '" << command << "'? (y/n): ";
  char answer = 'n';
  if (!(std::cin >> answer)) {
    std::cin.clear();
  }
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (answer != 'y' && answer != 'Y') {
    return "Command execution cancelled by user: " + command;
  }

  std::array<char, 256> buffer;
  std::string output;
  FILE* pipe = popen((command + " 2>&1").c_str(), "r");
  if (!pipe) {
    return "Error: could not run command: " + command;
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }

  int status = pclose(pipe);
  output += "\n[exit code: " + std::to_string(status) + "]";

  return output;
}
