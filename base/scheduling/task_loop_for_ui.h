#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_UI_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_UI_H_

// For now, TaskLoopForUI is just a pass-through to TaskLoopForWorker since we
// don't really support UI capabilities just yet. Once we implement a real UI
// thread along with UI event listening mechanics, then we can implement a real
// TaskLoopForUI, and swap out the implementation here.

#include "base/build_config.h"

#include "base/scheduling/task_loop_for_worker.h"

namespace base {

// TODO(domfarolino): Implement a real TaskLoopForUI and reference it here.
using TaskLoopForUI = TaskLoopForWorker;

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_FOR_UI_H_
