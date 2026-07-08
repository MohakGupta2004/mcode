#include "provider_manager.h"
#include "providers.h"
#include <memory>
#include <stdexcept>
#include <string>
Provider &ProviderManager::getCurrentProvider() { return *active_provider; }

void ProviderManager::registerProvider(const std::shared_ptr<Provider> &provider) {
  active_provider = provider;
}

void ProviderManager::setModel(const std::string &model_name) {
  if (!active_provider) {
    throw std::runtime_error("No provider registered");
  }
  active_provider->setModel(model_name);
}
