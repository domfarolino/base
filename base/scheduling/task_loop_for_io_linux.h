#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_IO_LINUX_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_IO_LINUX_H_

#include <map>

#include "base/scheduling/task_loop.h"

namespace base {

// Note that this `TaskLoop` variant only works on Linux, or platforms with
// `epoll`, as we use `eventfd` and `epoll` to block on OS-level notifications
// and IPC primitives.
class TaskLoopForIOLinux : public TaskLoop {
 public:
  // |SocketReader|s register themselves with a |TaskLoopForIO| associated with
  // a given file descriptor. They are notified asynchronously when the file
  // descriptor can be read from.
  class SocketReader {
   public:
    SocketReader(int fd) : fd_(fd) {}
    virtual ~SocketReader() = default;
    int Socket() { return fd_; }
    virtual void OnCanReadFromSocket() = 0;

   protected:
    int fd_;
  };

  TaskLoopForIOLinux();
  ~TaskLoopForIOLinux();

  TaskLoopForIOLinux(TaskLoopForIOLinux&) = delete;
  TaskLoopForIOLinux(TaskLoopForIOLinux&&) = delete;
  TaskLoopForIOLinux& operator=(const TaskLoopForIOLinux&) = delete;

  // Thread::Delegate implementation.
  void Run() override;
  // Can be called from any thread.
  void Quit() override;

  // TaskRunner::Delegate implementation.
  // Can be called from any thread.
  void PostTask(OnceClosure cb) override;

  void QuitWhenIdle() override;

  // Can be called from any thread.
  void WatchSocket(SocketReader* reader);
  void UnwatchSocket(SocketReader* reader);

  // Can be called from any thread (it is *implicitly* thread-safe).
  void Wakeup();

 private:
  // The epoll file descriptor that drives the task loop. This is only written
  // to once on whatever thread `this` loop is constructed on (note this may be
  // different than the thread that the loop ultimately gets bound to). Can be
  // used in `epoll_*()` calls from any thread.
  const int epollfd_;

  // Eventfd file descriptor used by `Wakeup()` to wake up the task loop in
  // response to `PostTask()`, `Quit()`, `QuitWhenIdle()`.
  //
  // It is created with the `EFD_SEMAPHORE` flag so that multiple wakeups (from
  // many e.g., PostTask()s, Quit()s, QuitWhenIdle()s) result in the internal
  // counter for the file descriptor being incremented each time. This results
  // in `epoll_wait()` always notifying `Run()` that there is something that
  // needs to be addressed, until we `read()` each message off of the file
  // descriptor, which in turn decrements the internal counter. If we did not
  // use semaphore semantics, the contents of the `read()` on the file
  // descriptor would tell us how many notifications `eventfd_wakeup_` has
  // received, which does not work cleanly with our `Run()` loop implementation.
  //
  // See https://man7.org/linux/man-pages/man2/eventfd.2.html.
  const int eventfd_wakeup_;

  // These are listeners that get notified when their file descriptor has been
  // written to and is ready to read from. |SocketReader|s are expected to
  // unregister themselves from this map upon destruction.
  // This data structure is guarded by |mutex_| as it can be written to from any
  // thread interacting with |this|.
  std::map<int, SocketReader*> async_socket_readers_;

  // See corresponding documentation above `TaskLoopForIOMac::event_count_`.
  size_t event_count_ = 1;
};

}  // namespace base

#endif  // BASE_SCHEDULING_TASK_LOOP_FOR_IO_LINUX_H_
