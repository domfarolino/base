#include <iostream>

#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"

int main() {
  std::shared_ptr<base::TaskLoop> main_loop =
      base::TaskLoop::Create(base::ThreadType::UI);

  base::GetCurrentThreadTaskRunner()->PostTask([](){
    std::cout << "Quitting the main thread TaskLoop from a posted task" << std::endl;
    base::GetCurrentThreadTaskLoop()->Quit();
  });

  main_loop->Run();
  return 0;
}
