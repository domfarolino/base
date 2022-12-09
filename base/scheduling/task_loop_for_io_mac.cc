#include "base/scheduling/task_loop_for_io_mac.h"

#include <mach/mach.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/socket.h>

#include <vector>

namespace base {

TaskLoopForIOMac::TaskLoopForIOMac() : kqueue_(kqueue()) {
  kern_return_t kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE, &wakeup_);
  CHECK_EQ(kr, KERN_SUCCESS);

  kevent64_s event{};
  event.ident = wakeup_;
  event.filter = EVFILT_MACHPORT;
  event.flags = EV_ADD;
  event.fflags = MACH_RCV_MSG;
  event.ext[0] = reinterpret_cast<uint64_t>(&wakeup_buffer_);
  event.ext[1] = sizeof(wakeup_buffer_);

  kr = kevent64(kqueue_, &event, 1, nullptr, 0, 0, nullptr);
  CHECK_EQ(kr, KERN_SUCCESS);
}

TaskLoopForIOMac::~TaskLoopForIOMac() {
  // CHECK(async_socket_readers_.empty());
}

void TaskLoopForIOMac::Run() {
  while (true) {
    // The last task that ran may have introduced a new |SocketReader| that
    // increased our |event_count_|, and thus the number of event filters that
    // we care about notifications for. In order for the kqueue to notify us of
    // multiple unique events (that are keyed on (ident, filter) pairs, see
    // https://www.manpagez.com/man/2/kevent64/) in one go, we expand our
    // |events_| vector, which is where kevent64 pushes potentially multiple
    // events that is has received. We of course could choose to not expand the
    // size of this vector, and then we'd only be able to service a single event
    // and source after every kevent64 call.
    events_.resize(event_count_);

    timespec timeout{0, 0};
    int rv = kevent64(/*kernal_queue=*/kqueue_, /*change_list=*/nullptr,
                      /*num_changes=*/0, /*event_list=*/events_.data(),
                      /*num_events=*/events_.size(), /*flags=*/0,
                      /*timeout=*/quit_when_idle_ ? &timeout : nullptr);
    // We grab a lock so that neither of the following are tampered with on
    // another thread while we are reading/writing them:
    //   - |queue_|
    //   - |async_socket_readers_|
    mutex_.lock();

    // At this point we have at least one event from the kernel, unless we're in
    // the |quit_when_idle_| mode and have nothing to do. We detect this
    // idleness via `rv == 0` later and break.
    CHECK(rv >= 1 || (quit_when_idle_ && rv == 0));

    // If `quit_` is set, that is a no-brainer, we have to just quit. But if
    // `quit_when_idle_` is set, we can *only* quit if we're idle. When
    // `quit_when_idle_` is set, `kevent64()` above does not block, and thus
    // `rv == 0` at this point indicates we are idle.
    // If `quit_when_idle_ && rv >= 1`, then we are not idle. This can happen in
    // a number of ways:
    //   - Common case when `quit_when_idle_ && rv == 1 && queue_.empty()`:
    //     This can happen if the loop is blocked in the `kevent64()` call
    //     above, and then `QuitWhenIdle()` is called. This wakes up the loop
    //     with a MACHPORT event, but puts nothing on the `queue_`. We don't
    //     break the loop here, but instead handle this below in the
    //     `EVFILT_MACHPORT` branch. If no socket reads or tasks are queued by
    //     the time the next loop iteration comes around, `kevent64()` will
    //     immediately return with `rv == 0` after not blocking and here we'll
    //     finally detect that we're idle and break.
    //   - Less common case when `quit_when_idle_ && rv == 1 && queue_.empty()`
    //     If the common case (above) happens, but before the second loop
    //     iteration starts at least one socket read gets queued before
    //     `kevent64()` is called, then `kevent64()` will return with `rv >= 1`
    //     In that case, we're not idle because we have reads to process so we
    //     won't break here.
    //   - Less common case when `quit_when_idle_ && rv == 1 && !queue_empty()`
    //     Same as above, except instead of a socket read being queued before
    //     the second loop iteration, a task gets posted.
    //   - It is also technically possible for: `kevent64()` to be blocking, and
    //     someone calls `QuitWhenIdle()` at the same time a task or a socket
    //     read is queued. In that case since `rv != 0` we won't break here, but
    //     will first process any events as usual.
    if (quit_ || (quit_when_idle_ && rv == 0)) {
      mutex_.unlock();
      break;
    }

    CHECK_GE(rv, 1);

    // Process any queued events (the number of which is `rv`).
    for (int i = 0; i < rv; ++i) {
      auto* event = &events_[i];

      if (event->filter == EVFILT_READ) {
        int fd = event->ident;

        auto* socket_reader = async_socket_readers_[fd];
        mutex_.unlock();

        CHECK(socket_reader);
        socket_reader->OnCanReadFromSocket();
      } else if (event->filter == EVFILT_MACHPORT) {

        // If the queue is empty but we have a MACHPORT event to process, this
        // is either:
        //   1.) A `QuitWhenIdle()` call waking us up with no actual work to do
        //   2.) A MACHPORT that was queued from an earlier `Quit()` call. That
        //       is, N `Quit()` calls in a row will make the next `Run()`
        //       immediately quit the loop. But the *next* `Run()` will still
        //       observe the N-1 queued MACHPORT events. That means we'll end up
        //       here, even though `!quit_ && !quit_when_idle_`.
        if (queue_.empty()) {
          mutex_.unlock();
          continue;
        }

        CHECK(queue_.size());
        OnceClosure cb = std::move(queue_.front());
        queue_.pop();
        mutex_.unlock();

        ExecuteTask(std::move(cb));
      } else {
        NOTREACHED();
      }
    } // for.

    // By this point, |mutex_| will always be unlocked so that we can lock it
    // for the next iteration. Note that we can't just unlock it here at the end
    // of this loop, to make things simple. We have to unlock it before we
    // process whatever event type we're processing or else we are prone to
    // deadlocks. For example, if we keep |mutex_| locked while we run a task
    // that we pull from the |queue_|, then if that task calls PostTask() on
    // this loop, then it will try and lock |mutex_| and deadlock forever.
  } // while (true).

  // We need to reset |quit_| when |Run()| actually completes, so that we can
  // call |Run()| again later.
  quit_ = false;
  quit_when_idle_ = false;
}

void TaskLoopForIOMac::WatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();

  mutex_.lock();
  // A socket reader can only be registered once.
  CHECK_EQ(async_socket_readers_.find(fd), async_socket_readers_.end());
  async_socket_readers_[fd] = socket_reader;
  event_count_++;
  mutex_.unlock();

  std::vector<kevent64_s> events;
  kevent64_s new_event{};
  new_event.ident = fd;
  new_event.flags = EV_ADD;
  new_event.filter = EVFILT_READ;
  events.push_back(new_event);

  // Invoke kevent64 not to listen to events, but to supply a changelist of
  // event filters that we're interested in being notified about from the
  // kernel.
  int rv = kevent64(/*kernel_queue=*/kqueue_, /*change_list=*/events.data(),
                    /*num_changes=*/events.size(), /*event_list=*/nullptr,
                    /*num_events=*/0, /*flags=*/0, /*timeout=*/nullptr);
  CHECK_GE(rv, 0);
}

void TaskLoopForIOMac::UnwatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();
  std::vector<kevent64_s> events;

  kevent64_s new_event{};
  new_event.ident = fd;
  new_event.flags = EV_DELETE;
  new_event.filter = EVFILT_READ;
  events.push_back(new_event);

  // Invoke kevent64 not to listen to events, but to supply a changelist of
  // event filters that we're interested in being notified about from the
  // kernel.
  int rv = kevent64(/*kernel_queue=*/kqueue_, /*change_list=*/events.data(),
                    /*num_changes=*/events.size(), /*event_list=*/nullptr,
                    /*num_events=*/0, /*flags=*/0, /*timeout=*/nullptr);
  CHECK_GE(rv, 0);

  mutex_.lock();
  CHECK(!async_socket_readers_.empty());
  async_socket_readers_.erase(fd);
  event_count_--;
  mutex_.unlock();
}

void TaskLoopForIOMac::PostTask(OnceClosure cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  mutex_.unlock();
  MachWakeup();
}

void TaskLoopForIOMac::Quit() {
  mutex_.lock();
  quit_ = true;
  mutex_.unlock();
  MachWakeup();
}

void TaskLoopForIOMac::QuitWhenIdle() {
  mutex_.lock();
  quit_when_idle_ = true;
  mutex_.unlock();
  MachWakeup();
}

void TaskLoopForIOMac::MachWakeup() {
  // Send an empty message to |wakeup_|. There are three things that can happen
  // here:
  //   1.) This is being called from another thread, and the task loop's thread
  //       is sleeping. In this case, the message will wake up the sleeping
  //       thread and it will execute the task we just pushed to the queue.
  //   2.) This is being called from another thread, and the task loop's thread
  //       is not sleeping, but running another task. In this case, the message
  //       will get pushed to the kernel queue to ensure that the thread doesn't
  //       sleep on its next iteration of its run loop, and instead executes the
  //       next task in the queue (possible this one, if there are no others
  //       before it).
  //   3.) This is being called from the thread that the task loop is running
  //       on. In that case, we're in the middle of a task right now, which is
  //       the same thing as (2) above.
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  message.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  message.header.msgh_remote_port = wakeup_;

  kern_return_t kr = mach_msg_send(&message.header);
  CHECK_EQ(kr, KERN_SUCCESS);
}

}; // namespace base
