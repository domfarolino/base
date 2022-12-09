#ifndef BASE_THREADING_SIMPLE_THREAD_H_
#define BASE_THREADING_SIMPLE_THREAD_H_

#include <memory>
#include <tuple>
#include <utility>

#include "base/callback.h"
#include "base/threading/thread.h"

namespace base {

class TaskRunner;

class SimpleThread : public Thread {
public:
  class SimpleThreadDelegate : public Thread::Delegate {
   public:
    explicit SimpleThreadDelegate(OnceClosure thread_function) :
        thread_function_(std::move(thread_function)) {}

    // Thread::Delegate implementation.
    void BindToCurrentThread(ThreadType) override {}
    void Run() override {
      CHECK(thread_function_);
      thread_function_();
    }
    std::shared_ptr<TaskRunner> GetTaskRunner() override { NOTREACHED(); }
    void Quit() override {}
    void QuitWhenIdle() override {}

   private:
    OnceClosure thread_function_;
  };

  template <typename F, typename... Args>
  SimpleThread(F func, Args... args) : Thread() {
    OnceClosure thread_function =
        BindOnce(std::forward<F>(func), std::forward<Args>(args)...);
    delegate_.reset(new SimpleThreadDelegate(std::move(thread_function)));

    // Start the thread with the overridden delegate.
    Start();
  }
};

} // namespace base

#endif // BASE_THREADING_SIMPLE_THREAD_H_
