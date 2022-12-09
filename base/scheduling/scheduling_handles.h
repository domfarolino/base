#ifndef BASE_SCHEDULING_SCHEDULING_HANDLES_H_
#define BASE_SCHEDULING_SCHEDULING_HANDLES_H_

#include <memory>

namespace base {

class TaskLoop;
class TaskRunner;

void SetUIThreadTaskLoop(std::weak_ptr<TaskLoop>);
void SetIOThreadTaskLoop(std::weak_ptr<TaskLoop>);
void SetCurrentThreadTaskLoop(std::weak_ptr<TaskLoop>);

std::shared_ptr<TaskLoop> GetUIThreadTaskLoop();
// TODO(domfarolino): Consider returning `TaskLoopForIO` directly.
std::shared_ptr<TaskLoop> GetIOThreadTaskLoop();
std::shared_ptr<TaskLoop> GetCurrentThreadTaskLoop();
std::shared_ptr<TaskRunner> GetCurrentThreadTaskRunner();

}; // namespace base

#endif // BASE_SCHEDULING_SCHEDULING_HANDLES_H_
