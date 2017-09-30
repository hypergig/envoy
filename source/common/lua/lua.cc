#include "common/lua/lua.h"

#include <iostream> // fixfix

#include "common/common/assert.h"

namespace Envoy {
namespace Lua {

static int envoy_msleep(lua_State* state) {
  return lua_yield(state, 0);
}

State::State(const std::string& code) {
  state_ = lua_open();
  luaL_openlibs(state_);

  lua_pushcfunction(state_, envoy_msleep);
  lua_setglobal(state_, "envoy_msleep");

  if (0 != luaL_loadstring(state_, code.c_str()) ||
      0 != lua_pcall(state_, 0, 0, 0)) {
    ASSERT(false);
  }
}

State::~State() {
  lua_close(state_);
}

void State::runThread(const std::string& thread_start) {
  lua_State* thread_state = lua_newthread(state_);
  lua_getglobal(thread_state, thread_start.c_str());

  int rc;
  while ((rc = lua_resume(thread_state, 0)) == LUA_YIELD);
  if (0 != rc) {
    std::cerr << lua_tostring(thread_state, -1);
    ASSERT(false);
  }
}

}
}
