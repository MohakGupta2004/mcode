#pragma once
#include "../conversation/conversation.h"
#include "../storage/storage.h"
#include "../providers/provider_manager.h"
#include <string>
class Commander {
  private:
  public:
    bool handle(std::string input, Conversation& conversation, Storage& storage, ProviderManager& provider);

    // Interactive line reader. Reads a full line in raw terminal mode so we can
    // intercept the '@' key and pop up a directory picker modal. Returns the
    // typed line. Sets `eof` to true if the user pressed Ctrl-D on an empty line.
    std::string readLine(const std::string& prompt, bool& eof);
};
