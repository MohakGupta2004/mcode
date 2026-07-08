#include "commander.h"
#include <iostream>
#include <stdexcept>
#include <string>

bool Commander::handle(std::string input, Conversation &conversation,
                       Storage &storage, ProviderManager &provider) {
  if (input.empty()) {
    return true;
  }

  if (input == "history") {
    conversation.printHistory();
    return true;
  }
  if (input == "clear") {
    conversation.clearHistory();
    return true;
  }
  if (input == "save") {
    storage.save(conversation);
    return true;
  }

  if (input.substr(0, 7) == "/model " || input=="/model") {
    try {
      if(input.size()<=7) {
        std::cout<<"Usage: /model <model-name>" <<std::endl;
      }
      std::string model_name = input.substr(7);
      provider.setModel(model_name);
      return true;
    } catch (const std::out_of_range e) {
      return true;
    } catch(const std::runtime_error e) {
      std::cout<<e.what()<<std::endl;
      return true;
    }
  }



  return false;
}
