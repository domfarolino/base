load("@rules_cc//cc:defs.bzl", "cc_library")

config_setting(
  name = "mac",
  constraint_values = ["@bazel_tools//platforms:osx"],
)

config_setting(
  name = "linux",
  constraint_values = ["@bazel_tools//platforms:linux"],
)

cc_library(
  name = "base",
  hdrs = [
    "build_config.h",
    "callback.h",
    "check.h",
    "scheduling/scheduling_handles.h",
    "scheduling/task_loop.h",
    "scheduling/task_loop_for_io.h",
    "scheduling/task_loop_for_ui.h",
    "scheduling/task_loop_for_worker.h",
    "scheduling/task_runner.h",
    "synchronization/condition_variable.h",
    "synchronization/mutex.h",
    "synchronization/synchronization_helpers.h",
    "threading/thread.h",
    "threading/thread_checker.h",
    "threading/simple_thread.h",
  ] +
  select({
    ":mac": ["scheduling/task_loop_for_io_mac.h"],
    ":linux": ["scheduling/task_loop_for_io_linux.h"],
  }),
  srcs = [
    "scheduling/scheduling_handles.cc",
    "scheduling/task_loop.cc",
    "scheduling/task_loop_for_worker.cc",
    "threading/thread.cc",
    "threading/thread_checker.cc",
  ] +
  select({
    ":mac": ["scheduling/task_loop_for_io_mac.cc"],
    ":linux": ["scheduling/task_loop_for_io_linux.cc"],
  }),

  deps = [],
  linkopts = select({
    ":mac": [],
    ":linux": ["-lpthread"],
  }),
  visibility = ["//visibility:public"],
)

cc_test(
  name = "base_tests",
  srcs = [
    "build_config_test.cc",
    "callback_test.cc",
    "scheduling/task_loop_test.cc",
    "threading/simple_thread_test.cc",
    "threading/thread_checker_test.cc",
    "threading/thread_test.cc",
    "scheduling/task_loop_for_io_test.cc",
    "scheduling/scheduling_handles_test.cc",
  ],
  copts = ["-Iexternal/gtest/include"],
  deps = [
    "//base:base",
    "@gtest//:gtest",
    "@gtest//:gtest_main",
  ],
)
