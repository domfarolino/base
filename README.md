# `//base` 🧱

![ci-shield](https://github.com/domfarolino/base/actions/workflows/workflow.yml/badge.svg)

Base is a cross-platform threading and task scheduling library written in C++.
It provides a lightweight event loop implementation to support asynchronous
programming patterns, and the ability to perform platform-independent I/O.

It was designed based on pieces of Chromium's [`//base`] directory, which is a
collection of threading, task scheduling, synchronization, process management,
platform-independent I/O primitives, and much more.

This library was originally written in the [domfarolino/browser] repository, and
later pulled out into this repository to be a more general dependency for other
projects.

----

### Table of contents

- [Supported platforms](#supported-platforms)
- [API reference](#api-reference)
- [Building and running the tests](#building-and-running-the-tests)
  - [Debugging](#debugging)

----


## Supported platforms

![Linux](./assets/linux.svg)
![macOS](./assets/apple.svg)

In progress:

![Windows](./assets/windows.svg)


## API reference

TODO.

## Building and running the tests

With the repository downloaded, to build and run the examples, run:

```sh
$ bazel build examples
$ ./bazel-bin/examples/<foo>
```

To run the tests, run one of the following:

```sh
$ bazel test base/base_tests
```

or...


```sh
$ bazel build base/mage_tests
$ ./bazel-bin/base/base_tests
```

### Debugging

The base library is built with debugging symbols by default (see
[`.bazelrc`](.bazelrc)). To debug a failing test or other internals with `lldb`,
run:

```sh
$ bazel build base/base_tests
$ lldb ./bazel-bin/base/base_tests
# Set breakpoints
$ br s -n TaskLoopForIO::Run
$ br s -f task_loop_for_io_test.cc -l <line_number>
$ run --gtest_filter="TaskLoopForIOTestBase.BasicSocketReading"


[`//base`]: https://source.chromium.org/chromium/chromium/src/+/main:base/
[domfarolino/browser]: https://github.com/domfarolino/browser
