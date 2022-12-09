#include "base/scheduling/scheduling_handles.h"

#include <memory>

#include "base/check.h"
#include "base/scheduling/task_loop.h"

namespace base {

// Process-global pointers (accessible from all threads in a process).
//
// These are weak process-global pointers to underlying |TaskLoop|s that we wish
// to reference but not keep alive.
static std::weak_ptr<TaskLoop> g_ui_task_loop;
static std::weak_ptr<TaskLoop> g_io_task_loop;

// Thread-global pointers (global only within a thread).
// Weak pointer for the same reason above.
static thread_local std::weak_ptr<TaskLoop> g_current_thread_task_loop;

void SetUIThreadTaskLoop(std::weak_ptr<TaskLoop> ui_task_loop) {
  g_ui_task_loop = ui_task_loop;
}

void SetIOThreadTaskLoop(std::weak_ptr<TaskLoop> io_task_loop) {
  g_io_task_loop = io_task_loop;
}

void SetCurrentThreadTaskLoop(std::weak_ptr<TaskLoop> task_loop) {
  g_current_thread_task_loop = task_loop;
}

// The getters only return std::shared_ptrs so that their callers don't have to
// go through the dance of dereferencing it from a std::weak_ptr. If the
// underlying object has been deleted, then the std::shared_ptrs returned from
// these methods will be falsy.
std::shared_ptr<TaskLoop> GetUIThreadTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_ui_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskLoop> GetIOThreadTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_io_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskLoop> GetCurrentThreadTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_current_thread_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskRunner> GetCurrentThreadTaskRunner() {
  std::shared_ptr<TaskLoop> task_loop = GetCurrentThreadTaskLoop();
  if (task_loop)
    return task_loop->GetTaskRunner();

  return nullptr;
}

}; // namespace base
