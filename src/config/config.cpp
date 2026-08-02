
#include "config.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

void Config::load() {

    const char* home = std::getenv("HOME");

#if defined(_WIN32) || defined(_WIN64)
    if (!home)
        home = std::getenv("USERPROFILE");
#endif

    if (!home) {
        std::cerr << "Could not determine home directory\n";
        return;
    }

    fs::path configDir =
        fs::path(home) / ".config" / "mcode";

    fs::path filepath =
        configDir / "config.json";

    try {

        if (fs::exists(filepath)) {
            std::ifstream f(filepath);
            config = json::parse(f);
        }
        else {
            fs::create_directories(configDir);

            std::ofstream file(filepath);

            file << R"({
    "API_KEY": {
        "claude":"",
        "openai":""
    }
})";

            std::cout << "Config file created\n";
            std::cout << "Edit:\n";
            std::cout << filepath << '\n';
        }

    }
    catch (const fs::filesystem_error& e) {
        std::cerr << e.what() << '\n';
    }
}
std::string Config::getApiKey(const std::string &provider) {
  std::string apikey;
  try {
    if (config["API_KEY"][provider] != NULL) {
      apikey = config["API_KEY"][provider];
    } else {
      throw std::runtime_error(
          "Couldn't able to parse API_KEY. Make sure you provide one.");
    }
  } catch (const std::runtime_error e) {
    std::cerr << e.what() << std::endl;
  } catch (const json::parse_error e) {
    std::cerr << e.what() << std::endl;
  }
  return apikey;
}
