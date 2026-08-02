#include <string>
#include "web_search.h"
#include <cpr/cpr.h>

namespace {
// Cap how much of a fetched page we hand back to the model - a raw HTML page
// can be megabytes, which would blow the context window for one tool call.
constexpr size_t kMaxBodyChars = 20000;
}

std::string WebSearch::execute(nlohmann::json& arguements) {
  if (!arguements.contains("url") || !arguements["url"].is_string()) {
    return "Error: missing required string argument 'url'";
  }

  std::string url = arguements["url"].get<std::string>();
  if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
    return "Error: url must start with http:// or https://";
  }

  cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{15000},
                            cpr::Redirect{true});

  if (r.error) {
    return "Error: request failed: " + r.error.message;
  }
  if (r.status_code == 0) {
    return "Error: could not reach '" + url + "'";
  }

  std::string body = r.text;
  bool truncated = false;
  if (body.size() > kMaxBodyChars) {
    body = body.substr(0, kMaxBodyChars);
    truncated = true;
  }

  std::string result = "[HTTP " + std::to_string(r.status_code) + "] " + url + "\n\n" + body;
  if (truncated) {
    result += "\n\n[...truncated]";
  }
  return result;
}
