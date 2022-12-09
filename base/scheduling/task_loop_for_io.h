#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_IO_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_IO_H_

// This file is just for bringing in platform-specific implementations of
// TaskLoopForIO, and merging them into a single type. Users of TaskLoopForIO
// should include this file, and it will "give" them the right TaskLoopForIO
// depending on the platform they're on. It is inspired by
// https://source.chromium.org/chromium/chromium/src/+/master:base/message_loop/message_pump_for_io.h.
//
// It is tempting to try and create one abstract interface to unify all
// platform-specific implementations of TaskLoopForIO, especially to ensure they
// all adhere to the same API contract, however it occurred to me that they will
// all not actually have the same APIs. For example, SocketReader currently
// deals in terms of file descriptors, but on Windows we'll have to use HANDLEs
// or something. In that case, the SocketReader API is not the same across
// platforms, so users of TaskLoopForIO will have to also base their usage of
// TaskLoopForIO off of the existence of platform-specific compiler directives.
// Luckily there will only be a small set of direct users of these kinds of
// platform-specific APIs.
//
// Note that it actually is possible to generalize the TaskLoopForIO API surface
// even across platforms, if we create platform-specific implementations of a
// "platform handle" concept, which itself has platform-specific code to
// conditionally deal in terms of POSIX file descriptors, Windows HANDLEs, or
// what have you, all while maintaining a platform agnostic API that
// TaskLoopForIO can understand. This would be worth looking into once we expand
// to Windows, if we ever do.

#include "base/build_config.h"

#if defined(OS_MACOS)
#include "base/scheduling/task_loop_for_io_mac.h"
#elif defined(OS_LINUX)
#include "base/scheduling/task_loop_for_io_linux.h"
#endif

namespace base {

#if defined(OS_MACOS)
using TaskLoopForIO = TaskLoopForIOMac;
#elif defined(OS_LINUX)
using TaskLoopForIO = TaskLoopForIOLinux;
#endif

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_FOR_IO_H_
