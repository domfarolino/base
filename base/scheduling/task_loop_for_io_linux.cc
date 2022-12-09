#include "base/scheduling/task_loop_for_io_linux.h"

#include <stdint.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

namespace base {

TaskLoopForIOLinux::TaskLoopForIOLinux()
    : epollfd_(epoll_create1(0)), eventfd_wakeup_(eventfd(0, EFD_SEMAPHORE)) {
  CHECK_NE(epollfd_, -1);
  CHECK_NE(eventfd_wakeup_, -1);

  // Register `eventfd_wakeup_` with the epoll.
  epoll_data_t event_data;
  event_data.fd = eventfd_wakeup_;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data = event_data;

  int rv = epoll_ctl(epollfd_, EPOLL_CTL_ADD, eventfd_wakeup_, &ev);
  CHECK_NE(rv, -1);
}

TaskLoopForIOLinux::~TaskLoopForIOLinux() {
  CHECK_NE(close(eventfd_wakeup_), -1);
  CHECK_NE(close(epollfd_), -1);
  CHECK(async_socket_readers_.empty());
}

void TaskLoopForIOLinux::Run() {
  while (true) {
    CHECK_GE(event_count_, 1);
    struct epoll_event events[event_count_];
    int timeout = quit_when_idle_ ? 0 : -1;
    int rv = epoll_wait(epollfd_, events, event_count_, timeout);

    // We grab a lock so that neither of the following are tampered with on
    // another thread while we are reading/writing them:
    //   - |queue_|
    //   - |async_socket_readers_|
    mutex_.lock();

    // At this point we have at least one event from the kernel, unless we're in
    // the |quit_when_idle_| mode and have nothing to do. We detect this
    // idleness via `rv == 0` later and break.
    CHECK(rv >= 1 || (quit_when_idle_ && rv == 0));

    // See documentation above this code in `TaskLoopForIOMac`.
    if (quit_ || (quit_when_idle_ && rv == 0)) {
      mutex_.unlock();
      break;
    }

    CHECK_GE(rv, 1);

    // Process any queued events (the number of which is `rv`).
    for (int i = 0; i < rv; ++i) {
      struct epoll_event* event = &events[i];
      int fd = event->data.fd;
      if (fd == eventfd_wakeup_) {
        // Read a message off of `eventfd_wakeup_`, which will decrement the
        // internal counter of messages on that file descriptor, since it is in
        // `EFD_SEMAPHORE` mode.
        uint64_t msg = 0;
        int read_rv = read(eventfd_wakeup_, &msg, sizeof(msg));
        CHECK_NE(read_rv, -1);

        // See corresponding documentation above this code in
        // `TaskLoopForIOMac`.
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
        auto* socket_reader = async_socket_readers_[fd];
        mutex_.unlock();
        CHECK(socket_reader);
        socket_reader->OnCanReadFromSocket();
      }
    }

    // See corresponding documentation at this location in `TaskLoopForIOMac`.
  }  // while (true).

  // We need to reset |quit_| when |Run()| actually completes, so that we can
  // call |Run()| again later.
  quit_ = false;
  quit_when_idle_ = false;
}

void TaskLoopForIOLinux::WatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();

  mutex_.lock();
  // A socket reader can only be registered once.
  CHECK_EQ(async_socket_readers_.find(fd), async_socket_readers_.end());
  async_socket_readers_[fd] = socket_reader;
  CHECK_GE(event_count_, 1);
  event_count_++;
  // Protect against overflow.
  CHECK_GE(event_count_, 1);
  mutex_.unlock();

  epoll_data_t event_data;
  event_data.fd = fd;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data = event_data;

  int rv = epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev);
  CHECK_NE(rv, -1);
}

void TaskLoopForIOLinux::UnwatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();

  epoll_data_t event_data;
  event_data.fd = fd;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data = event_data;

  int rv = epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
  CHECK_NE(rv, -1);

  mutex_.lock();
  CHECK(!async_socket_readers_.empty());
  async_socket_readers_.erase(fd);
  event_count_--;
  CHECK_GE(event_count_, 1);
  mutex_.unlock();
}

void TaskLoopForIOLinux::PostTask(OnceClosure cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  mutex_.unlock();
  Wakeup();
}

void TaskLoopForIOLinux::Quit() {
  mutex_.lock();
  quit_ = true;
  mutex_.unlock();
  Wakeup();
}

void TaskLoopForIOLinux::QuitWhenIdle() {
  mutex_.lock();
  quit_when_idle_ = true;
  mutex_.unlock();
  Wakeup();
}

void TaskLoopForIOLinux::Wakeup() {
  // Send an empty message to |eventfd_wakeup_|. See corresponding documentation
  // in `TaskLoopForIOMac::Wakeup()`.
  //
  // The value of what we write doesn't matter since `eventfd_wakeup_` is in
  // `EFD_SEMAPHORE` mode. See documentation above `eventfd_wakeup_`.
  uint64_t msg = 1;
  ssize_t rv = write(eventfd_wakeup_, &msg, sizeof(msg));
  CHECK(rv != 0);
}

};  // namespace base
