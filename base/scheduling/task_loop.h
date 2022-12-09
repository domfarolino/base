#ifndef BASE_SCHEDULING_TASK_LOOP_H_
#define BASE_SCHEDULING_TASK_LOOP_H_

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/scheduling/task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"
#include "base/threading/thread.h"

namespace base {

// |TaskLoop| is owned long-term, solely by the owner of |Thread::Delegate|
// (concretely, that's |Thread|). However, we have to inherit from
// |std::enable_shared_from_this| so that we can hand std::weak_ptr<TaskLoop>s
// out to |TaskRunner|. We should never handing std::shared_ptr<TaskLoop>s out
// to anyone, and the std::weak_ptr<TaskLoop>s owned by |TaskRunner| should only
// should ever take ownership of this object for the brief period of time it
// needs to post a task. For more information see the documentation above
// |TaskRunner|.
class TaskLoop : public Thread::Delegate,
                 public TaskRunner::Delegate,
                 public std::enable_shared_from_this<TaskLoop> {
 public:
  TaskLoop() = default;
  virtual ~TaskLoop() = default;

  TaskLoop(TaskLoop&) = delete;
  TaskLoop(TaskLoop&&) = delete;
  TaskLoop& operator=(const TaskLoop&) = delete;

  static std::shared_ptr<TaskLoop> CreateUnbound(ThreadType type);
  // TODO(domfarolino): Consider renaming this CreateBound() for extra clarity.
  static std::shared_ptr<TaskLoop> Create(ThreadType type);

  // Thread::Delegate implementation.
  // This method does two things:
  //   1. Binds process-global |TaskLoop| pointers for well-known threads.
  //      Depending on the type of TaskLoop |this| is, we may bind |this| to the
  //      process-global TaskLoop handle corresponding to |type|. This is so
  //      that from a process's UI thread, you can get a |TaskLoop| or
  //      |TaskRunner| reference to the same process's IO thread, and vice
  //      versa. We don't do this for ThreadType::WORKER threads/loops, because
  //      there could be an arbitrary number of these threads in a process,
  //      whereas the UI/IO threads are considered well-known, and unique within
  //      a process.
  //   2. Binds thread-global |TaskLoop| and |TaskRunner| pointers.
  //      We bind |this| and |this->GetTaskRunner()| to the corresponding
  //      thread_local "global" pointers. This allows all tasks running on the
  //      current thread to grab a |TaskLoop| (aka |this|), or |TaskRunner|
  //      reference associated with |this| for convenience. This is done
  //      for all thread types, since the results are only visible with in the
  //      current thread.
  void BindToCurrentThread(ThreadType type) override;
  // Called on the thread that |this| is bound to.
  void Run() override = 0;
  // Can be called from any thread.
  void Quit() override = 0;
  // Can be called from any thread. Just like |Quit()|, but instead of setting
  // |quit_| to true, we set |quit_when_idle_| to true, which only quits the run
  // loop if it has no tasks to process. This method basically turns this
  // instance of the loop into a run-until-idle loop. The reason we introduce it
  // is so that we can let the loop run for a while (potentially idling along
  // the way), and then post some tasks to finish up, and wait for those tasks
  // to finish. This is most useful in a multi-thread environment.
  void QuitWhenIdle() override = 0;

  // Can be called from any thread.
  std::shared_ptr<TaskRunner> GetTaskRunner() override;

  // TaskRunner::Delegate implementation.
  // Can be called from any thread.
  void PostTask(OnceClosure cb) override = 0;

  // Called on the thread that |this| is bound to. Just like |Run()|, except it
  // runs the loop until the underlying event queue is empty or until a task
  // quits the loop. Once empty or quit, this method returns as opposed to
  // waiting indefinitely.
  void RunUntilIdle();

  // TODO(domfarolino): This should return a `RepeatingClosure` once something
  // like that exists.
  virtual OnceClosure QuitClosure();

 protected:
  void ExecuteTask(OnceClosure cb) {
    cb();
  }

  // Each concrete implementation of |TaskLoop| has its own members that control
  // when/on what the loop blocks, and how it is woken up. This base class has
  // no members specific to the internals of the loop, besides:
  //  - |mutex_|: A base::Mutex
  //  - |queue_|: A task queue to hold base::Callbacks (tasks), since all
  //              |TaskLoop| implementations support task posting. Access is
  //              guarded by |mutex_|
  //  - |quit_|: A boolean set by |Quit()|. |Run()| is the only reader of this,
  //             meaning the thread that |this| is bound to is the only thread
  //             that reads this variable. |Run()| uses this in every loop
  //             iteration to determine, regardless of whether there are
  //             outstanding tasks or events to process, if the loop should stop
  //             running.
  //  - |quit_when_idle_|: A boolean set by both |RunUntilIdle()| and
  //                       |QuitWhenIdle()|. |Run()| is the only reader of this
  //                       variable, and it is used specifically when there are
  //                       no tasks/events to process, to quit the loop instead
  //                       of let it idle and wait for more tasks/events to
  //                       process.

  // Used to lock |queue_|, since it can be accessed from multiple threads via
  // |PostTask()|.
  base::Mutex mutex_;
  std::queue<OnceClosure> queue_;
  bool quit_ = false;
  bool quit_when_idle_ = false;

 private:
  std::weak_ptr<TaskLoop> GetWeakPtr() {
    return shared_from_this();
  }
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_H_
