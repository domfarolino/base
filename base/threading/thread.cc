#include "base/threading/thread.h"

#include <pthread.h>
#include <unistd.h>

#include <chrono>
#include <memory>

#include "base/check.h"
#include "base/scheduling/task_loop.h"

namespace base {

Thread::Thread(ThreadType type) : type_(type) {
  pthread_attr_init(&attributes_);
}

Thread::~Thread() {
  Stop();
}

void Thread::Start() {
  CHECK(!started_via_api_);
  started_via_api_ = true;

  // Give subclasses a chance to override their own |delegate_|.
  if (!delegate_) {
    delegate_.reset();
    delegate_ = TaskLoop::CreateUnbound(type_);
  }

  CHECK(delegate_);
  pthread_create(&id_, &attributes_, ThreadFunc, this);
}

std::shared_ptr<TaskRunner> Thread::GetTaskRunner() {
  if (delegate_)
    return delegate_->GetTaskRunner();

  // |delegate_| may be null even before Stop()/join() are called if the
  // underlying delegate quits itself.
  return nullptr;
}

void Thread::Stop() {
  StopImpl(/*wait_for_idle=*/false);
}
void Thread::StopWhenIdle() {
  StopImpl(/*wait_for_idle=*/true);
}

void Thread::StopImpl(bool wait_for_idle) {
  // We can't CHECK(started_via_api_) here because Stop() should be idempotent.
  // If |started_via_api_| is false then we don't want to do anything here
  // because that means we've already called Stop() or join() (or we've never
  // called Start()), in which case:
  //   - |delegate_| has already been reset, so there is nothing to Quit()
  //   - The backing thread has already been terminated, so there is nothing to join
  if (!started_via_api_) {
    CHECK(!delegate_);
    return;
  }

  // If we get here, then we know |Start()| has been called at least once, but
  // |delegate_| may be null if it has quit itself and already terminated the
  // backing thread.
  if (delegate_) {
    if (wait_for_idle)
      delegate_->QuitWhenIdle();
    else
      delegate_->Quit();
  }

  join();
}

// static
void Thread::sleep_for(std::chrono::milliseconds ms) {
  usleep(ms.count() * 1000);
}

void Thread::join() {
  pthread_join(id_, nullptr);

  // It is possible that the underlying physical thread actually stopped running
  // before this |join()| call e.g., if a task running on said thread ran
  // |base::GetCurrentThreadTaskLoop()->Quit()| at some point. In that case, we
  // reset |delegate_| but we don't reset |started_via_api_|, because if we did
  // that would encourage what we consider to be an abuse of this API. For
  // example, we want to discourage the following sequence:
  //   1.) [Main Thread]: Creates a base::Thread, and Start()s it.
  //   2.) [Other Thread]: Eventually quits itself, stopping the run loop
  //   3.) [Main Thread]: Listens to some side effect of [Other Thread] quitting
  //       itself so that it knows it can call |Start()| again to re-run the
  //       thread.
  // Listening to side effects of the backing thread terminated is fine, but we
  // want to prevent the main thread from calling Start() again before calling
  // calling Stop() or join(), in case it misread the side effects of the
  // backing thread terminating. If you think a base::Thread has stopped
  // running, you must confirm this the base::Thread API via |join()| or |Stop()|.
  started_via_api_ = false;
}

void Thread::RegisterDelegateResetCallbackForTesting(OnceClosure cb) {
  delegate_reset_callback_for_testing_ = std::move(cb);
}

// static
void* Thread::ThreadFunc(void* in) {
  Thread* thread = (Thread*)(in);
  CHECK(thread);

  thread->ThreadMain();

  thread->delegate_.reset();
  if (thread->delegate_reset_callback_for_testing_)
    thread->delegate_reset_callback_for_testing_();

  pthread_exit(0);
}

// This method is run for the duration of the physical thread's lifetime. When
// it exits, the thread is terminated.
void Thread::ThreadMain() {
  CHECK(delegate_);
  delegate_->BindToCurrentThread(type_);
  delegate_->Run();
}

} // namespace base
