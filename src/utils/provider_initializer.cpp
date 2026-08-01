#include "provider_initializer.h"
#include "providers/openrouter_provider.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <vector>

namespace {
// Used only if getModels() comes back empty (e.g. request failed).
constexpr const char* kFallbackModel = "tencent/hy3:free";
}

void initializeProviders(ProviderManager& provider) {
  provider.registerProvider(std::make_shared<OpenRouter>());

  std::vector<std::string> models = provider.getCurrentProvider().getModels();

  std::string model = kFallbackModel;
  if (!models.empty()) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    int maxIndex = std::min<int>(static_cast<int>(models.size()) - 1, 3);
    int index = std::rand() % (maxIndex + 1);
    model = models[index];
  }

  provider.setModel(model);
}
