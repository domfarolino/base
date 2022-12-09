#include "base/scheduling/task_loop.h"

#include <memory>

#include "base/build_config.h"
#include "base/check.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/scheduling/task_loop_for_ui.h"
#include "base/scheduling/task_loop_for_worker.h"

namespace base {

void TaskLoop::BindToCurrentThread(ThreadType type) {
  // Depending on |type|, create a process-global pointer to |this|.
  switch (type) {
    case ThreadType::UI:
      CHECK(!GetUIThreadTaskLoop());
      SetUIThreadTaskLoop(GetWeakPtr());
      CHECK(GetUIThreadTaskLoop());
      break;
    case ThreadType::IO:
      CHECK(!GetIOThreadTaskLoop());
      SetIOThreadTaskLoop(GetWeakPtr());
      CHECK(GetIOThreadTaskLoop());
      break;
    case ThreadType::WORKER:
      // We don't store process-global pointers to |ThreadType::WORKER|
      // TaskLoops because there can be more than one of these per process.
      break;
  }

  // Regardless of |type|, set the current thread's TaskLoop to |this|, and
  // TaskRunner to |this->GetTaskRunner()|.
  SetCurrentThreadTaskLoop(GetWeakPtr());
}

// static
std::shared_ptr<TaskLoop> TaskLoop::CreateUnbound(ThreadType type) {
  switch (type) {
    case ThreadType::UI:
      return std::shared_ptr<TaskLoopForUI>(new TaskLoopForUI());
    case ThreadType::IO:
      return std::shared_ptr<TaskLoopForIO>(new TaskLoopForIO());
    case ThreadType::WORKER:
      return std::shared_ptr<TaskLoopForWorker>(new TaskLoopForWorker());
  }

  NOTREACHED();
  return std::shared_ptr<TaskLoopForWorker>();
}

// static
std::shared_ptr<TaskLoop> TaskLoop::Create(ThreadType type) {
  std::shared_ptr<TaskLoop> task_loop = CreateUnbound(type);
  task_loop->BindToCurrentThread(type);
  return task_loop;
}

std::shared_ptr<TaskRunner> TaskLoop::GetTaskRunner() {
  return std::shared_ptr<TaskRunner>(new TaskRunner(GetWeakPtr()));
}

void TaskLoop::RunUntilIdle() {
  QuitWhenIdle();
  Run();
}

OnceClosure TaskLoop::QuitClosure() {
  return BindOnce(&TaskLoop::Quit, this);
}

}; // namespace base
