#include <cpr/cprtypes.h>
#include <cpr/response.h>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include "config/config.h"
#include "conversation/conversation.h"
#include "providers/openrouter_provider.h"
#include "providers/provider_manager.h"
#include "storage/storage.h"
#include "commander/commander.h"
#include <cpr/cpr.h>
#include <utility>
int main() {
    Conversation conversation;
    Storage storage;
    Commander handler;
    Config config;
    ProviderManager provider;
    storage.load(conversation);
    provider.registerProvider(std::make_shared<OpenRouter>());
    provider.setModel("openai/gpt-4o-mini");
    config.load();
    while(true) {
      std::string input;
      std::cout<<"> ";
      std::getline(std::cin, input);
      if(input == "exit") {
        break;
      }
      if(handler.handle(input, conversation,storage, provider))  {
        continue;
      }
      conversation.addMessage({
        "user", input
      });
      std::pair<std::string, std::string> response = provider.getCurrentProvider().ask(conversation.getMessage(), config);
      conversation.addMessage({"assistant", response.first});
    }
    return 0;
}
