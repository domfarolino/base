#include "base/threading/thread_checker.h"

#include "base/check.h"
#include "base/scheduling/scheduling_handles.h"

namespace base {

ThreadChecker::ThreadChecker() {
  std::shared_ptr<TaskLoop> current_task_loop = base::GetCurrentThreadTaskLoop();
  CHECK(current_task_loop);
  current_task_loop_ = base::GetCurrentThreadTaskLoop();
}

bool ThreadChecker::CalledOnConstructedThread() {
  return base::GetCurrentThreadTaskLoop() == current_task_loop_.lock();
}

} // namespace base
