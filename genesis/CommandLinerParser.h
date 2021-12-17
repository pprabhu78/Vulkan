#pragma once

#include <vector>
#include <string>
#include <unordered_map>

namespace genesis
{
   class CommandLineParser
   {
   public:
      struct CommandLineOption {
         std::vector<std::string> commands;
         std::string value;
         bool hasValue = false;
         std::string help;
         bool set = false;
      };
      std::unordered_map<std::string, CommandLineOption> options;
      CommandLineParser();
      void add(std::string name, std::vector<std::string> commands, bool hasValue, std::string help);
      void printHelp();
      void parse(std::vector<const char*> arguments);
      bool isSet(std::string name);
      std::string getValueAsString(std::string name, std::string defaultValue);
      int32_t getValueAsInt(std::string name, int32_t defaultValue);
   };
}