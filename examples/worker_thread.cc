#include <iostream>

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/thread.h"

class TaskParam {
 public:
  TaskParam(int data): data_(data) {}
  TaskParam(const TaskParam& other) {
    data_ = other.data_;
  }

  int get_data() {
    return data_;
  }

 private:
  int data_;
};

// These run off-main-thread.
void TaskOne() {
  std::cout << std::endl << "I'm TaskOne, and I'm being invoked on a worker "
                            "thread" << std::endl;
}
void TaskTwo(int input) {
  std::cout << std::endl << "I'm TaskTwo, and I'm being invoked with intetger: "
                         << input << std::endl;
}
void TaskThree(TaskParam param) {
  std::cout << std::endl << "I'm TaskThree, and I'm being invoked with object "
                            "whose data is: " << param.get_data() << std::endl;
}

// This runs on the main thread.
void PostTasksToWorkerThread(base::Thread& thread) {
  std::shared_ptr<base::TaskRunner> task_runner = thread.GetTaskRunner();
  int task_number;
  while (1) {
    std::cout << "Enter 1, 2, or 3 to post tasks 1, 2, or 3 respectively. Or "
                 "0 to quit: ";
    std::cin >> task_number;

    if (!task_number)
      break;

    if (task_number < 0 || task_number > 3) {
      std::cout << "Invalid number" << std::endl;
      continue;
    }

    if (task_number == 1) {
      task_runner->PostTask(base::BindOnce(&TaskOne));
    } else if (task_number == 2) {
      int input;
      std::cout << "Enter TaskTwo's input: ";
      std::cin >> input;
      task_runner->PostTask(base::BindOnce(&TaskTwo, input));
    } else {
      int input;
      std::cout << "Enter TaskThree's input: ";
      std::cin >> input;
      TaskParam param(input);
      base::OnceClosure task  = base::BindOnce(&TaskThree, param);
      task_runner->PostTask(std::move(task));
    }
  }

  thread.Stop();
}

int main() {
  base::Thread worker_thread;
  worker_thread.Start();
  PostTasksToWorkerThread(worker_thread);
  worker_thread.join();
  return 0;
}
