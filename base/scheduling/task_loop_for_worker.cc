#include "base/scheduling/task_loop_for_worker.h"

namespace base {

void TaskLoopForWorker::Run() {
  while (true) {
    cv_.wait(mutex_, [&]() -> bool{
      bool has_tasks = !queue_.empty();
      bool can_skip_waiting = (has_tasks || quit_ || quit_when_idle_);
      return can_skip_waiting;
    });

    // We now own the lock for |queue_|, until we explicitly release it.
    if (quit_ || (quit_when_idle_ && queue_.empty())) {
      cv_.release_lock();
      break;
    }

    CHECK(queue_.size());
    OnceClosure cb = std::move(queue_.front());
    queue_.pop();
    cv_.release_lock();

    ExecuteTask(std::move(cb));
  }

  // We need to reset |quit_| when |Run()| actually completes, so that we can
  // call |Run()| again later.
  quit_ = false;
  quit_when_idle_ = false;
}

void TaskLoopForWorker::PostTask(OnceClosure cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  mutex_.unlock();

  cv_.notify_one();
}

void TaskLoopForWorker::Quit() {
  mutex_.lock();
  quit_ = true;
  mutex_.unlock();

  cv_.notify_one();
}

void TaskLoopForWorker::QuitWhenIdle() {
  mutex_.lock();
  quit_when_idle_ = true;
  mutex_.unlock();

  cv_.notify_one();
}

}; // namespace base
