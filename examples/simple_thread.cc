#include <iostream>

#include "base/threading/simple_thread.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"

// This runs on a worker thread, and posts tasks back to the main thread.
void PostTasksBackToMainThread(
    std::shared_ptr<base::TaskRunner> main_thread_task_runner) {
  std::cout << "START" << std::endl;
  for (int i = 0; i < 5; ++i) {
    main_thread_task_runner->PostTask([](){
      std::cout << "Main thread: Getting a ping from the worker thread" << std::endl;
    });
    base::Thread::sleep_for(std::chrono::milliseconds(500));
  }

  main_thread_task_runner->PostTask([](){
    std::cout << "Main thread: Getting the final ping from the worker "
                 "thread" << std::endl;
    // Since this is run on the main thread, this will quit the main thread's
    // TaskLoop.
    base::GetCurrentThreadTaskLoop()->Quit();
  });
}

int main() {
  std::shared_ptr<base::TaskLoop> main_loop =
      base::TaskLoop::Create(base::ThreadType::UI);

  // Spin up a worker thread that will perform some work and post tasks to the
  // main thread while we're waiting for the worker thread to finish. Just as an
  // example, we'll block the main thread until the work is done, solely by
  // running our loop and waiting for its result.
  base::SimpleThread simple(
      PostTasksBackToMainThread, main_loop->GetTaskRunner());
  main_loop->Run();

  simple.join();
  return 0;
}
