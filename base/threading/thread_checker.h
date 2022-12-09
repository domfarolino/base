#ifndef BASE_THREADING_THREAD_CHECKER_H_
#define BASE_THREADING_THREAD_CHECKER_H_

#include <memory>

#include "base/scheduling/scheduling_handles.h"
#include "base/threading/thread.h"

namespace base {

// This is a light-weight macro that can be used to assert the current thread
// type, as opposed to the heavier `ThreadChecker` class below which is
// typically used as a member function in classes that want to regularly assert
// that the thread their methods are being invoked on is the same thread that
// the object was constructed on.
//
// `ThreadChecker` can be used to check that a given object is used on the
// "correct" thread regardless of thread type, where "correct" is defined as the
// thread that the `ThreadChecker` (and likely the owning object) was
// constructed on.
#define CHECK_ON_THREAD(thread_type) \
switch (thread_type) { \
  case base::ThreadType::UI: \
    CHECK(base::GetCurrentThreadTaskLoop()); \
    CHECK_EQ(base::GetUIThreadTaskLoop(), base::GetCurrentThreadTaskLoop()); \
    break; \
  case base::ThreadType::IO: \
    CHECK(base::GetCurrentThreadTaskLoop()); \
    CHECK_EQ(base::GetIOThreadTaskLoop(), base::GetCurrentThreadTaskLoop()); \
    break; \
  case base::ThreadType::WORKER: \
    NOTREACHED(); \
    break; \
}

class TaskLoop;

class ThreadChecker {
 public:
  ThreadChecker();

  bool CalledOnConstructedThread();

 private:
  std::weak_ptr<TaskLoop> current_task_loop_;
};

} // namespace base

#endif // BASE_THREADING_THREAD_CHECKER_H_
