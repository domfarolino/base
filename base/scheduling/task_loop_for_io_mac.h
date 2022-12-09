#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_IO_MAC_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_IO_MAC_H_

#include <mach/mach.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/event.h>

#include <map>
#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/scheduling/task_loop.h"
#include "base/synchronization/mutex.h"

namespace base {

// Note that this |TaskLoop| variant only works on macOS, as it uses Mach ports
// and |kevent64()| to block on OS-level IPC primitives.
class TaskLoopForIOMac : public TaskLoop {
 public:
  // |SocketReader|s register themselves with a |TaskLoopForIO| associated with
  // a given file descriptor. They are notified asynchronously when the file
  // descriptor can be read from.
  class SocketReader {
   public:
    SocketReader(int fd): fd_(fd) {}
    virtual ~SocketReader() = default;
    int Socket() { return fd_; }
    virtual void OnCanReadFromSocket() = 0;
   protected:
    int fd_;
  };

  TaskLoopForIOMac();
  ~TaskLoopForIOMac();

  TaskLoopForIOMac(TaskLoopForIOMac&) = delete;
  TaskLoopForIOMac(TaskLoopForIOMac&&) = delete;
  TaskLoopForIOMac& operator=(const TaskLoopForIOMac&) = delete;

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
  void MachWakeup();

 private:
  // The kqueue that drives the task loop. This is only written to once on
  // whatever thread |this| loop is constructed on (note this may be different
  // than the thread it is ultimately bound to). Can be read from any thread.
  const int kqueue_;

  // Receive right to which an empty Mach message is sent to wake up the loop
  // in response to |PostTask()|.
  mach_port_t wakeup_;
  // Scratch buffer that is used to receive the message sent to |wakeup_|.
  mach_msg_empty_rcv_t wakeup_buffer_;

  // These are listeners that get notified when their file descriptor has been
  // written to and is ready to read from. |SocketReader|s are expected to
  // unregister themselves from this map upon destruction.
  // This data structure is guarded by |mutex_| as it can be written to from any
  // thread interacting with |this|.
  std::map<int, SocketReader*> async_socket_readers_;

  // NOTE ABOUT THREAD SAFETY:
  //   * The below |event_count_| can be modified from any thread that interacts
  //     with |this| loop, and therefore its writes are guarded by |mutex_| so
  //     that all writes persist.
  //   * The below |events_| is only ever written to and read from the thread
  //     that |this| loop is bound to, in the Run() method.
  //   * Reading from these variables does not need to be synchronized. In
  //     Run(), we read from |event_count_| to resize |events_|. You can imagine
  //     that once we read |event_count_| to resize |events_| (for submission to
  //     `kevent64()`), we're only taking a snapshot of the value. It could have
  //     changed by the time we _actually_ call `kevent64()` to wait for events,
  //     to either greater or less than the one we snapshotted. Let's consider
  //     each case separately.
  //       1.) Value was increased after snapshot:
  //           For example, we enter the loop in Run() with |event_count_| set
  //           to 4 (three socket readers + our wakeup event). We resize
  //           |events_| accordingly, but before we call `kevent64()` to wait
  //           for at most 4 events, two more readers are added, and the actual
  //           value of |event_count_| gets updated to 6 and the underlying
  //           kernel queue knows about 6 possible event sources. We call
  //           `kevent64()` with the argument `num_events=4`, meaning we'll
  //           return with at 4 events ready to process from the 6 event
  //           sources. This is totally valid! The kqueue system actually lets
  //           you process a single event at a time with num_events=1 if you
  //           want, it's just less efficient.
  //       2.) Value was decreased after snapshot:
  //           Similar to the above example, we invoke `kevent64()` with
  //           `num_events=x` even though the number of event sources that the
  //           underlying kernel queue actually knows about is x-2, for example.
  //           In this case, the number of events we are willing to accept is
  //           larger than the number of events the kernel queue will ever
  //           produce, which is also fine. You can try setting |event_count_|
  //           to an absurdly high number below and running the tests.
  // END NOTE
  // The number of event types (filters) that we're interested in listening to
  // via |kqueue_|. There is always at least 1, for the |wakeup_| port (or
  // |port_set_|), but increase this count whenever we register a new e.g.,
  // |SocketReader|.
  size_t event_count_ = 1;
  // This buffer is where events from the kernel queue are stored after calls to
  // |kevent64|. This buffer is consulted in |Run()|, where events are pulled
  // and the loop responds to them.
  std::vector<kevent64_s> events_{event_count_};
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_FOR_IO_MAC_H_
