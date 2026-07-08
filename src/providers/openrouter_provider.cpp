#include "openrouter_provider.h"
#include <cpr/cpr.h>
#include <cpr/response.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

std::pair<std::string, std::string> OpenRouter::ask(const std::vector<Message> &history, Config &config) {
  const std::string API_KEY = config.getApiKey("openrouter");
  // TODO: implement OpenRouter API request (model = getModel()).
  return {"", ""};
}

// Gateway name, constant. All models are reached through this single gateway.
std::string OpenRouter::getName() const { return "openrouter"; }

void OpenRouter::setModel(const std::string &model) { this->model = model; }

std::string OpenRouter::getModel() const { return model; }
