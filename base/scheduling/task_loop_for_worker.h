#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_WORKER_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_WORKER_H_

#include <queue>

#include "base/callback.h"
#include "base/scheduling/task_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"

namespace base {

class TaskLoopForWorker : public TaskLoop {
public:
  TaskLoopForWorker() = default;
  ~TaskLoopForWorker() = default;

  TaskLoopForWorker(TaskLoopForWorker&) = delete;
  TaskLoopForWorker(TaskLoopForWorker&&) = delete;
  TaskLoopForWorker& operator=(const TaskLoopForWorker&) = delete;

  // Thread::Delegate implementation.
  void Run() override;
  // Can be called from any thread.
  void Quit() override;

  // TaskRunner::Delegate implementation.
  // Can be called from any thread.
  void PostTask(OnceClosure cb) override;

  void QuitWhenIdle() override;

private:
  // This |TaskLoop| implementation only responds to user-posted tasks, so we
  // use a condition variable to wake up the loop when a task has been posted
  // and is ready to execute.
  base::ConditionVariable cv_;
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_FOR_WORKER_H_
