#include "ollama_provider.h"
#include <cpr/cpr.h>
#include <cpr/response.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {
constexpr const char* kBaseUrl = "http://localhost:11434";

// Ollama 400s if `tools` is attached to a model that doesn't support tool
// calling (e.g. completion-only models). Check /api/show first so we can
// silently drop the field instead of failing the request.
bool modelSupportsTools(const std::string& model) {
  json payload = {{"model", model}};
  auto r = cpr::Post(cpr::Url{std::string(kBaseUrl) + "/api/show"},
                     cpr::Header{{"Content-Type", "application/json"}},
                     cpr::Body{payload.dump()});
  if (r.status_code != 200) {
    return false;
  }
  json j = json::parse(r.text, nullptr, false);
  if (j.is_discarded() || !j.contains("capabilities")) {
    return false;
  }
  for (const auto& cap : j["capabilities"]) {
    if (cap == "tools") {
      return true;
    }
  }
  return false;
}
} // namespace

std::pair<std::string, std::string>
Ollama::ask(const std::vector<Message> &history, Config &config, ToolManager& tools) {
  // Local daemon, no API key required.

  std::string responseBuffer;
  std::string lineBuffer;
  std::string rawBuffer; // raw bytes, for error reporting (WriteCallback bypasses r.text)
  json toolCallsAccum = json::array();

  auto write_callback = [&](const std::string_view &data, intptr_t) -> bool {
    rawBuffer += data;
    lineBuffer += data;
    size_t pos;
    while ((pos = lineBuffer.find('\n')) != std::string::npos) {
      std::string line = lineBuffer.substr(0, pos);
      lineBuffer.erase(0, pos + 1);
      if (line.empty()) {
        continue;
      }
      auto j = json::parse(line, nullptr, false);
      if (j.is_discarded() || !j.contains("message")) {
        continue;
      }
      const auto &message = j["message"];

      if (message.contains("content") && message["content"].is_string()) {
        std::string chunk = message["content"].get<std::string>();
        std::cout << chunk << std::flush;
        responseBuffer += chunk;
      }

      // Ollama sends each tool call whole (no fragment merging needed).
      if (message.contains("tool_calls")) {
        for (const auto &tc : message["tool_calls"]) {
          toolCallsAccum.push_back(tc);
        }
      }
    }
    return true;
  };

  json messages = json::array();
  for (const auto &m : history) {
    messages.push_back({{"role", m.role}, {"content", m.content}});
  }

  const json tool_defs = tools.getDefinitions();
  const bool tools_supported = !tool_defs.empty() && modelSupportsTools(model);

  constexpr int MAX_TOOL_ITERATIONS = 8;
  for (int iteration = 0; iteration < MAX_TOOL_ITERATIONS; ++iteration) {
    responseBuffer.clear();
    lineBuffer.clear();
    rawBuffer.clear();
    toolCallsAccum.clear();

    json json_payload = {{"model", model},
                         {"messages", messages},
                         {"stream", true}};
    if (tools_supported) {
      json_payload["tools"] = tool_defs;
    }

    auto r = cpr::Post(cpr::Url{std::string(kBaseUrl) + "/api/chat"},
                       cpr::Header{{"Content-Type", "application/json"}},
                       cpr::Body{json_payload.dump()},
                       cpr::WriteCallback{write_callback});

    if (r.status_code != 200) {
      std::cerr << "HTTP Error " << r.status_code << ": "
                << (rawBuffer.empty() ? r.text : rawBuffer) << "\n";
      break;
    }

    if (toolCallsAccum.empty()) {
      break;
    }

    json assistant_msg = {{"role", "assistant"}};
    assistant_msg["content"] = responseBuffer;
    assistant_msg["tool_calls"] = toolCallsAccum;
    messages.push_back(assistant_msg);

    for (const auto &tc : toolCallsAccum) {
      const std::string name = tc["function"]["name"].get<std::string>();
      json args = tc["function"]["arguments"];
      std::string result = tools.execute(args, name);
      std::cout << "\n[tool: " << name << "]\n" << std::flush;
      messages.push_back({{"role", "tool"}, {"content", result}});
    }
  }

  std::cout << std::endl;
  return {responseBuffer, ""};
}

std::string Ollama::getName() const { return "ollama"; }

void Ollama::setModel(const std::string &model) { this->model = model; }

std::string Ollama::getModel() const { return model; }

std::vector<std::string> Ollama::getModels() const {
  auto r = cpr::Get(cpr::Url(std::string(kBaseUrl) + "/api/tags"));
  if (r.status_code != 200) {
    std::cerr << r.text << std::endl;
  }
  std::vector<std::string> list;
  json j = json::parse(r.text, nullptr, false);
  if (j.is_discarded() || !j.contains("models")) {
    return list;
  }
  for (auto i : j["models"]) {
    list.push_back(i["name"]);
  }
  return list;
}
