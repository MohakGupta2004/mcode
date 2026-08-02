#include <cpr/cprtypes.h>
#include <cpr/response.h>
#include <iostream>
#include <memory>
#include "config/config.h"
#include "conversation/conversation.h"
#include "providers/provider_manager.h"
#include "storage/storage.h"
#include "commander/commander.h"
#include "tools/tool_manager.h"
#include "utils/tool_initializer.h"
#include "utils/provider_initializer.h"
#include <cpr/cpr.h>
#include <utility>
static void printBanner() {
    std::cout <<
        "  __  __  ____ ___  ____  _____\n"
        " |  \\/  |/ ___/ _ \\|  _ \\| ____|\n"
        " | |\\/| | |  | | | | | | |  _|\n"
        " | |  | | |__| |_| | |_| | |___\n"
        " |_|  |_|\\____\\___/|____/|_____|\n\n";
}

int main() {
    printBanner();
    Conversation conversation;
    Storage storage;
    Commander handler;
    Config config;
    ProviderManager provider;
    ToolManager tool_manager;

    storage.load(conversation);
    initializeProviders(provider);
    initializeTools(tool_manager);

    config.load();
    while(true) {
      bool eof = false;
      std::string input = handler.readLine("> ", eof);
      if(eof || input == "exit") {
        break;
      }
      if(handler.handle(input, conversation,storage, provider))  {
        continue;
      }
      conversation.addMessage({
        "user", input
      });
      std::pair<std::string, std::string> response = provider.getCurrentProvider().ask(conversation.getMessage(), config, tool_manager);
      conversation.addMessage({"assistant", response.first});
      // Persist after every turn instead of only on the explicit "save"
      // command, so a later crash or a plain exit never loses history.
      storage.save(conversation);
    }
    return 0;
}
