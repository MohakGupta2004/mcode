#pragma once

#include "providers.h"
#include <string>
class OpenRouter:public Provider {
  private:
    std::string model;

  public:
    std::pair<std::string, std::string> ask(const std::vector<Message>& history, Config& config, ToolManager& tools) override;
    std::string getName() const override;
    void setModel(const std::string& model) override;
    std::string getModel() const override;
    std::vector<std::string> getModels() const override;
};
