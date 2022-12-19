load("@rules_cc//cc:defs.bzl", "cc_binary")

filegroup(
  name = "examples",
  srcs = [
    "bind",
    "producer_consumer",
    "simple_thread",
    "task_posting",
    "worker_thread",
  ],
)

cc_binary(
  name = "bind",
  srcs = ["bind.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "producer_consumer",
  srcs = ["producer_consumer.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "simple_thread",
  srcs = ["simple_thread.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "task_posting",
  srcs = ["task_posting.cc"],
  deps = ["//base:base"],
)
cc_binary(
  name = "worker_thread",
  srcs = ["worker_thread.cc"],
  deps = ["//base:base"],
)
