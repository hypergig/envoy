licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "lua_lib",
    srcs = ["lua.cc"],
    hdrs = ["lua.h"],
    external_deps = [
        "luajit",
    ],
    deps = [
        "//source/common/common:assert_lib",
    ],
)
