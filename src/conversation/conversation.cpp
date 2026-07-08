#include "conversation.h"
#include <iostream>
#include <vector>
void Conversation::addMessage(const Message &msg) { message.push_back(msg); }
void Conversation::printHistory() const {
  for (auto &m : message) {
    std::cout << m.role << " : " << m.content << std::endl;
  }
}

void Conversation::clearHistory() { message.clear(); }

std::vector<Message> Conversation::getMessage() const { return message; }
