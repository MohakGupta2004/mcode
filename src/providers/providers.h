#pragma once


#include <string>
#include <vector>
#include "../config/config.h"
#include "../models/message.h"
class Provider {
  public:
    virtual std::pair<std::string, std::string> ask(const std::vector<Message> &history, Config& config) = 0;

    // Gateway identifier (e.g. "openrouter"). This is the provider/gateway name,
    // NOT a model name. A single gateway can serve many models.
    virtual std::string getName() const = 0;

    // Model selection within the gateway. OpenRouter routes to many models, so the
    // model is a separate axis from the provider name and must not collide with it.
    virtual void setModel(const std::string& model) = 0;
    virtual std::vector<std::string> getModels() const = 0;
    virtual ~Provider()=default;
};
