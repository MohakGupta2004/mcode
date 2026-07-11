#pragma once
#include <string>
#include <nlohmann/json.hpp>
class Tool {
  public:
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

    // JSON-Schema object describing the tool's arguments. Return an empty
    // object schema (`{"type":"object","properties":{}}`) for a no-arg tool.
    virtual nlohmann::json getParameters() const = 0;

    virtual std::string execute(nlohmann::json& body) = 0;

    // OpenAI/OpenRouter tool definition built from the pieces above. Providers
    // consume this and never need to know about concrete tool types, so adding
    // a new tool is just a new subclass + registration, no provider changes.
    nlohmann::json getDefinition() const {
      return {
        {"type", "function"},
        {"function", {
          {"name", getName()},
          {"description", getDescription()},
          {"parameters", getParameters()}
        }}
      };
    }

    virtual ~Tool()=default;
};
