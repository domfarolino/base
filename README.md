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
  - [Task scheduling](#task-scheduling)
    - [`TaskLoop`](#taskloop)
    - [`TaskRunner`](#taskrunner)
    - [Thread-local & process-global scheduling handles](#thread-local--process-global-scheduling-handles)
  - [Threading](#threading)
    - [`Thread`](#thread)
    - [`SimpleThread`](#simplethread)
    - [`ThreadChecker`](#threadchecker)
  - [Synchronization](#synchronization)
    - [`ConditionVariable`](#conditionvariable)
    - [`Mutex`](#mutex)
  - [Callbacks](#callbacks)
    - [`OnceClosure` / `BindOnce`](#onceclosure--bindonce)
  - [Miscellaneous](#miscellaneous)
    - [`CHECK` variants](#check-variants)
- [Building and running the tests](#building-and-running-the-tests)
  - [Debugging](#debugging)

----


## Supported platforms

![Linux](./assets/linux.svg)
![macOS](./assets/apple.svg)

In progress:

![Windows](./assets/windows.svg)


## API reference

### Task scheduling

#### `TaskLoop`

`base::TaskLoop` is the abstract base class that defines the API for a standard
event loop that runs on a thread. It has three variants:
 - `TaskLoopForUI`
 - `TaskLoopForIO`
 - `TaskLoopForWorker`

TaskLoops primarily process "tasks", which are just function closures
(specifically `base::OnceClosure`s).

`TaskLoopForWorker` is the simplest implementation, only supporting the ability
to post and execute tasks asynchronously; it doesn't support any
platform-specific UI or I/O functionality. The `TaskLoopForUI` and
`TaskLoopForIO` variants are strict supersets of `TaskLoopForWorker`, supporting
the same task posting functionality as the worker flavor, but with additional
platform-specific UI and I/O listening capabilities respectively[^1].

TaskLoops can be created with one of the two static creation methods:
 - `CreateUnbound(ThreadType)`
 - `Create(ThreadType)`

Both of these take a `base::ThreadType` that describe one of the variants above,
and create and return the corresponding task loop implementation. The difference
is that `CreateUnbound()` does not "bind" the loop to the current thread-local
and process-global scheduling handles (see that section below), while the
`Create()` method does, and therefore **must** be called on the thread that the
task loop will actually run on. The reason these exist is so that when you spin
up a new thread, the intiator thread can create an unbound task loop (which can
immediately start queueing tasks) which is later bound on the new physical
thread.

To actually run a task loop, you simply call `Run()`. This must be called on the
thread that the loop is bound to, and will synchronously spin the loop, running
tasks as they come in and performantly waiting when there are no more tasks to
process.

The `Quit()` method can be called from any thread, and prompts the loop to quit
right away or after the currently-running task ends if any such task exists.

`QuitWhenIdle()` is similar to `Quit()`, but will quit the loop as soon as there
are no more tasks to execute, stopping it from waiting indefinitely for more
tasks.

`GetTaskRunner()` returns a `std::shared_ptr<TaskRunner>` associated with a task
loop; it is used to post tasks to the current loop, and it is safe to do so even
if the loop has been destroyed. This is the mechanism with which you should post
all tasks.

#### `TaskRunner`

`base::TaskRunner` is the mechanism with which you post tasks to `TaskLoop`s. It
has a single method `PostTask()` which does just that.

You typically hold a `std::shared_ptr<TaskRunner>` which has a weak reference to
the associated task loop, and therefore it is safe to call `PostTask()` even if
the underlying task loop has been destroyed. `TaskRunner`s are cheap and thread
safe, which means you can:
 - Have multiple `TaskRunner`s associated with a single `TaskLoop`, each living
   on their own thread and posting tasks whenever they want [✅ recommended]
 - Have a single task runner accessed from various threads, posting tasks
   whenever they want [supported, but weird... not recommended]

#### Thread-local & process-global scheduling handles

The base library has a number of thread-local and process-global handles
accessible through static global functions.

**TODO**: finish this and the rest of the API reference.

### Threading

#### `Thread`

`base::Thread` is the principal API with which you create a new physical thread
with a running `TaskLoop`. This

#### `SimpleThread`
#### `ThreadChecker`

### Synchronization

#### `ConditionVariable`
#### `Mutex`

### Callbacks

#### `OnceClosure` / `BindOnce`

### Miscellaneous

#### `CHECK` variants


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
$ bazel build base/base_tests
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
```

[^1]: Right now since `//base` is in an early development phase, no
platform-specific UI work has been implemented, and `TaskLoopForUI` is just an
alias for `TaskLoopForWorker`. Once the base library supports platform-specific
UI integration, that should change.

[`//base`]: https://source.chromium.org/chromium/chromium/src/+/main:base/
[domfarolino/browser]: https://github.com/domfarolino/browser
