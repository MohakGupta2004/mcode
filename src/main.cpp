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
static const std::string kSystemPrompt =
    "You are mcode, a terminal coding agent. Use tools to inspect and "
    "modify the user's project; don't just describe what you would do.\n\n"
    "command_executor runs each command in a brand-new shell process - "
    "the working directory does NOT persist between separate calls. If a "
    "task needs a directory change (e.g. \"go up a directory\", \"go into "
    "src\"), chain it with && in the SAME call, e.g. 'cd .. && git status' "
    "or 'cd src && ls'. Never invent a literal path like \"/previous "
    "directory\" from phrases such as \"previous directory\" or \"parent "
    "directory\" - translate those to real shell syntax (.., ~, etc).\n\n"
    "If a tool call fails or a command returns a nonzero exit code, don't "
    "stop or ask the user first - diagnose from the error output and retry "
    "with a corrected or alternative command. Try up to 2-3 different "
    "approaches (fix a typo, use a different flag, use an equivalent tool) "
    "before giving up and explaining the failure to the user.";

static void printBanner() {
    std::cout <<
        "  __  __  ____ ___  ____  _____\n"
        " |  \\/  |/ ___/ _ \\|  _ \\| ____|\n"
        " | |\\/| | |  | | | | | | |  _|\n"
        " | |  | | |__| |_| | |_| | |___\n"
        " |_|  |_|\\____\\___/|____/|_____|\n\n"
        "A simplest terminal coding agent built using C++\n\n";
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
      std::vector<Message> messages = conversation.getMessage();
      messages.insert(messages.begin(), {"system", kSystemPrompt});
      std::pair<std::string, std::string> response = provider.getCurrentProvider().ask(messages, config, tool_manager);
      conversation.addMessage({"assistant", response.first});
      // Persist after every turn instead of only on the explicit "save"
      // command, so a later crash or a plain exit never loses history.
      storage.save(conversation);
    }
    return 0;
}
