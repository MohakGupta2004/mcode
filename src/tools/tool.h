#pragma once
#include <string>
#include <nlohmann/json.hpp>
class Tool {
  public: 
    virtual std::string getName() const = 0; 
    virtual std::string getDescription() const = 0;

    virtual std::string execute(nlohmann::json& body) = 0;

    virtual ~Tool()=default;
};
