#pragma once

#include "providers.h"
#include <memory>
#include <string>
class ProviderManager {
    private:
      std::shared_ptr<Provider> active_provider;
    public:
      // Register the single gateway provider (OpenRouter) and make it active.
      void registerProvider(const std::shared_ptr<Provider>& provider);
      // Select a model on the active gateway. Does not switch providers.
      void setModel(const std::string& model_name);
      Provider& getCurrentProvider();
};
