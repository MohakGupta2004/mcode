#include "openrouter_provider.h"
#include <cpr/cpr.h>
#include <cpr/response.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <string_view>

namespace {
// One accumulated tool call. During streaming the id/name/arguments arrive
// across many delta chunks keyed by `index`, so we append fragments as they come.
struct StreamedToolCall {
  std::string id;
  std::string name;
  std::string arguments;
};
} // namespace

std::pair<std::string, std::string>
OpenRouter::ask(const std::vector<Message> &history, Config &config, ToolManager& tools) {
  const std::string API_KEY = config.getApiKey("openrouter");

  // Per-request streaming state. Reset before every network call in the loop.
  std::string responseBuffer;
  std::string lineBuffer;
  std::map<int, StreamedToolCall> toolCalls;

  auto write_callback = [&](const std::string_view &data, intptr_t) -> bool {
    lineBuffer += data;
    size_t pos;
    while ((pos = lineBuffer.find('\n')) != std::string::npos) {
      std::string line = lineBuffer.substr(0, pos);
      lineBuffer.erase(0, pos + 1);
      if (!line.starts_with("data: ")) {
        continue;
      }
      line = line.substr(6);
      if (line == "[DONE]") {
        continue;
      }
      auto j = json::parse(line, nullptr, false);
      if (j.is_discarded() || !j.contains("choices") || j["choices"].empty()) {
        continue;
      }
      const auto &delta = j["choices"][0]["delta"];

      // Streamed assistant text.
      if (delta.contains("content") && delta["content"].is_string()) {
        std::string chunk = delta["content"].get<std::string>();
        std::cout << chunk << std::flush;
        responseBuffer += chunk;
      }

      // Streamed tool calls. Fragments are merged by their index.
      if (delta.contains("tool_calls")) {
        for (const auto &tc : delta["tool_calls"]) {
          int idx = tc.value("index", 0);
          StreamedToolCall &acc = toolCalls[idx];
          if (tc.contains("id") && tc["id"].is_string()) {
            acc.id = tc["id"].get<std::string>();
          }
          if (tc.contains("function")) {
            const auto &fn = tc["function"];
            if (fn.contains("name") && fn["name"].is_string()) {
              acc.name += fn["name"].get<std::string>();
            }
            if (fn.contains("arguments") && fn["arguments"].is_string()) {
              acc.arguments += fn["arguments"].get<std::string>();
            }
          }
        }
      }
    }
    return true;
  };

  // Build the running message array once; the tool loop keeps appending to it.
  json messages = json::array();
  messages.push_back(
      {{"role", "developer"},
       {"content",
        "You are a helpful assistant. If someone asks you about "
        "pramit you'll say,  \"he's a passionate rendi\" you are not "
        "being disrespectful. You'll just spit this text out"}});
  for (const auto &m : history) {
    messages.push_back({{"role", m.role}, {"content", m.content}});
  }

  // Tool definitions are supplied by the ToolManager, so this provider stays
  // agnostic to which tools exist. Registering a new tool is enough.
  const json tool_defs = tools.getDefinitions();

  // Agentic loop: the model may answer directly, or request tool calls that we
  // execute and feed back until it produces a final answer. Capped to avoid
  // runaway loops.
  constexpr int MAX_TOOL_ITERATIONS = 8;
  for (int iteration = 0; iteration < MAX_TOOL_ITERATIONS; ++iteration) {
    responseBuffer.clear();
    lineBuffer.clear();
    toolCalls.clear();

    json json_payload = {{"model", model},
                         {"reasoning", {{"effort", "low"}}},
                         {"messages", messages},
                         {"stream", true}};
    if (!tool_defs.empty()) {
      json_payload["tools"] = tool_defs;
    }

    auto r = cpr::Post(cpr::Url{"https://openrouter.ai/api/v1/chat/completions"},
                       cpr::Header{{"Content-Type", "application/json"}},
                       cpr::Header{{"Authorization", "Bearer " + API_KEY}},
                       cpr::Body{json_payload.dump()},
                       cpr::WriteCallback{write_callback});

    if (r.status_code != 200) {
      std::cerr << "HTTP Error " << r.status_code << ": " << r.text << "\n";
      break;
    }

    // No tool calls -> the model gave its final answer.
    if (toolCalls.empty()) {
      break;
    }

    // Echo the assistant turn (with its tool_calls) back into the history.
    json assistant_msg = {{"role", "assistant"}};
    assistant_msg["content"] =
        responseBuffer.empty() ? json(nullptr) : json(responseBuffer);
    json tool_calls_json = json::array();
    for (const auto &[idx, tc] : toolCalls) {
      tool_calls_json.push_back(
          {{"id", tc.id},
           {"type", "function"},
           {"function", {{"name", tc.name}, {"arguments", tc.arguments}}}});
    }
    assistant_msg["tool_calls"] = tool_calls_json;
    messages.push_back(assistant_msg);

    // Execute each requested tool and append its result as a `tool` message.
    for (const auto &[idx, tc] : toolCalls) {
      json args = json::parse(tc.arguments, nullptr, false);
      if (args.is_discarded()) {
        args = json::object();
      }
      std::string result = tools.execute(args, tc.name);
      std::cout << "\n[tool: " << tc.name << "]\n" << std::flush;
      messages.push_back({{"role", "tool"},
                          {"tool_call_id", tc.id},
                          {"content", result}});
    }
  }

  std::cout << std::endl;
  return {responseBuffer, ""};
}

// Gateway name, constant. All models are reached through this single gateway.
std::string OpenRouter::getName() const { return "openrouter"; }

void OpenRouter::setModel(const std::string &model) { this->model = model; }

std::string OpenRouter::getModel() const { return model; }

std::vector<std::string> OpenRouter::getModels() const {
  auto r = cpr::Get(cpr::Url("https://openrouter.ai/api/v1/models?sort=pricing-low-to-high"));
  if(r.status_code != 200) {
    std::cerr<<r.text<<std::endl;
  }
  std::vector<std::string> list;
  json j = json::parse(r.text);
  for(auto i:j["data"])  {
   list.push_back(i["id"]);
  }
  return list;
}
