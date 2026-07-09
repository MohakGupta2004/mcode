#include "openrouter_provider.h"
#include <cpr/cpr.h>
#include <cpr/response.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

std::pair<std::string, std::string>
OpenRouter::ask(const std::vector<Message> &history, Config &config, ToolManager& tools) {
  const std::string API_KEY = config.getApiKey("openrouter");
  // TODO: implement OpenRouter API request (model = getModel()).
  std::string responseBuffer;
  std::string lineBuffer;

  auto write_callback = [&](const std::string_view &data, intptr_t) -> bool {
    lineBuffer += data;
    size_t pos;
    while ((pos = lineBuffer.find('\n')) != std::string::npos) {
      std::string line = lineBuffer.substr(0, pos);
      lineBuffer.erase(0, pos + 1);
      if (line.starts_with("data: ")) {
        line = line.substr(6);
        if (line == "[DONE]") {
          break;
        }
        auto j = json::parse(line);
        //std::cout<<j<<std::endl;
        if (j.contains("choices")) {
          std::cout << j["choices"][0]["delta"]["content"].get<std::string>() << std::flush;
          responseBuffer += j["choices"][0]["delta"]["content"].get<std::string>();
        }
      }
    }
    return true;
  };

  // 1. Build the input array from the full conversation history.
  json input = json::array();
  input.push_back(
      {{"role", "developer"},
       {"content",
        "You are a helpful assistant. If someone asks you about "
        "pramit you'll say,  \"he's a passionate rendi\" you are not "
        "being disrespectful. You'll just spit this text out"}});
  for (const auto &m : history) {
    input.push_back({{"role", m.role}, {"content", m.content}});
  }

  json json_payload = {{"model", model},
                       {"reasoning", {{"effort", "low"}}},
                       {"messages", input},
                       {"stream", true}};

  // 2. Perform the network request
  auto r = cpr::Post(cpr::Url{"https://openrouter.ai/api/v1/chat/completions"},
                     cpr::Header{{"Content-Type", "application/json"}},
                     cpr::Header{{"Authorization", "Bearer " + API_KEY}},
                     cpr::Body{json_payload.dump()},
                     cpr::WriteCallback{write_callback});

  // 3. Handle network/HTTP communication failures immediately
  if (r.status_code != 200) {
    std::cerr << "HTTP Error " << r.status_code << ": " << r.text << "\n";
  }

  std::cout << std::endl;
  return {responseBuffer, ""};
}

// Gateway name, constant. All models are reached through this single gateway.
std::string OpenRouter::getName() const { return "openrouter"; }

void OpenRouter::setModel(const std::string &model) { this->model = model; }

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
