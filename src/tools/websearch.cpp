#include <string>
#include "web_search.h"

std::string WebSearch::execute(nlohmann::json& arguements) {
  if (!arguements.contains("url") || !arguements["url"].is_string()) {
    return "Error: missing required string argument 'url'";
  }

  std::string url = arguements["url"].get<std::string>();
  return "Web search URL: " + url;
}
