#ifndef BASE_SCHEDULING_SCHEDULING_HANDLES_H_
#define BASE_SCHEDULING_SCHEDULING_HANDLES_H_

#include <memory>

// We cannot forward declare the `TaskLoopForIO` type because it is redefined at
// compile-time by the following include, which cannot be captured by a raw
// declaration.
#include "base/scheduling/task_loop_for_io.h"

namespace base {

class TaskLoop;
class TaskRunner;

void SetUIThreadTaskLoop(std::weak_ptr<TaskLoop>);
void SetIOThreadTaskLoop(std::weak_ptr<TaskLoopForIO>);
void SetCurrentThreadTaskLoop(std::weak_ptr<TaskLoop>);

std::shared_ptr<TaskLoop> GetUIThreadTaskLoop();
std::shared_ptr<TaskLoopForIO> GetIOThreadTaskLoop();
std::shared_ptr<TaskLoop> GetCurrentThreadTaskLoop();
std::shared_ptr<TaskRunner> GetCurrentThreadTaskRunner();

}; // namespace base

#endif // BASE_SCHEDULING_SCHEDULING_HANDLES_H_
