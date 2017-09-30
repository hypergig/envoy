#pragma once

#include <string>

#include "luajit-2.0/lua.hpp"

namespace Envoy {
namespace Lua {

class State {
public:
  State(const std::string& code);
  ~State();

  void runThread(const std::string& thread_start);

private:
  lua_State* state_;
};

}
}
