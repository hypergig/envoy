#include "common/lua/lua.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Lua {

const std::string code(R"EOF(
  function envoy_on_decode_headers ()
    print("hello world before yield")
    envoy_msleep(1000)
    print("hello world after yield")
  end
)EOF");

TEST(Lua, Basic) {
  State s(code);
  s.runThread("envoy_on_decode_headers");
}

}
}
