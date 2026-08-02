#pragma once
#include "../conversation/conversation.h"
#include "../storage/storage.h"
#include "../providers/provider_manager.h"
#include <string>
#include <vector>
class Commander {
  private:
    // In-session prompt history for Up/Down recall, shell-style. `historyIndex_`
    // points at the entry currently shown (== history_.size() means "not
    // browsing, showing the live draft"). `draft_` stashes the in-progress line
    // the moment the user first presses Up, so Down can restore it later.
    std::vector<std::string> history_;
    size_t historyIndex_ = 0;
    std::string draft_;

  public:
    bool handle(std::string input, Conversation& conversation, Storage& storage, ProviderManager& provider);

    // Interactive line reader. Reads a full line in raw terminal mode so we can
    // intercept the '@' key and pop up a directory picker modal, and Up/Down to
    // recall previous prompts from this session. Returns the typed line. Sets
    // `eof` to true if the user pressed Ctrl-D on an empty line.
    std::string readLine(const std::string& prompt, bool& eof);
};
