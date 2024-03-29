#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <pthread.h>
#include <unistd.h>

#include <chrono>
#include <memory>

#include "base/callback.h"
#include "base/check.h"

namespace base {

class TaskRunner;

// The |ThreadType| of a given thread defines what its underlying task loop is
// capable of.
enum class ThreadType {
  // This type of thread *only* supports executing manually posted tasks, e.g.,
  // via |TaskRunner::PostTask()|.
  WORKER,

  // This type of thread supports waiting for and serializing native UI events
  // from windowing libraries, in addition to manually posted tasks.
  UI,

  // This type of thread supports asynchronous IO in addition to manually posted
  // tasks. Asynchronous IO consists of message pipes specific to the underlying
  // platform, as well as file descriptors. This thread type should be used by
  // IPC implementations.
  IO,
};

class Thread {
 public:
  Thread(ThreadType type = ThreadType::WORKER);
  ~Thread();

  // This is what the thread uses to actually run.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void BindToCurrentThread(ThreadType) = 0;
    virtual void Run() = 0;
    virtual std::shared_ptr<TaskRunner> GetTaskRunner() = 0;
    virtual void Quit() = 0;
    virtual void QuitWhenIdle() = 0;
  };

  void Start();
  // May return null if the |delegate_| does not provide task runners or has
  // already quit itself and terminated its backing thread.
  std::shared_ptr<TaskRunner> GetTaskRunner();
  void Stop();
  void StopWhenIdle();

  // C++ thread similar methods.
  static void sleep_for(std::chrono::milliseconds ms);
  void join();

  void RegisterBackingThreadTerminateCallbackForTesting(OnceClosure cb);

 protected:
  // These methods run on the newly-created thread.
  // This method is run for the duration of the backing thread's lifetime. When
  // it exits, the thread is terminated.
  static void* ThreadFunc(void* in);
  // This method invokes |Delegate::Run()| on the the backing physical thread.
  void ThreadMain();

  // NOTE: Never hand out any std::shared_ptr copies of this member! This is
  // only a std::shared_ptr so that it can produce std::weak_ptrs of itself.
  // The intention is to have |delegate_| be uniquely owned by |this|, but to be
  // able to create many new |TaskRunner| objects that all weakly reference
  // |delegate_|. See the documentation above |TaskRunner| for more information.
  // This member is accessed both on the thread owning |this|, and the backing
  // thread that |this| manages, however it is only ever assigned/reset on the
  // thread owning `this`, and is thread-safe even without synchronization
  // because its reset only takes place after the backing thread has been
  // terminated/joined. It is set and reset in the following ways:
  //   - Start() => creates a new |delegate_|
  //   - Stop()/join() => `delegate_` is reset here, only after the backing
  //     thread officially terminates
  std::shared_ptr<Delegate> delegate_;

 private:
  void StopImpl(bool wait_for_idle);

  ThreadType type_;
  pthread_t id_;
  pthread_attr_t attributes_;

  // Only ever accessed on the thread owning |this|. NEVER accessed on the
  // backing thread that |this| manages / represents. This is not always true
  // when |delegate_| is set and false when |delegate_| is not set. If the
  // backing thread that we manage terminates itself, |delegate_| will be reset
  // but |started_via_api_| will be true. It is only reset in |join()|, because
  // we require a call to Stop()/join() before calling Start() again.
  bool started_via_api_ = false;

  // When supplied by a test, this callback is the very last thing run on the
  // backing thread that `this` represents. It is run immediately after the
  // backing thread's delegate run loop ends, and just before it officially
  // terminates/exits. It is used to test the base::Thread API surface
  // immediately after its backing thread terminates but before `Stop()` is
  // called. An alternative would be to just post a task to the backing thread
  // to `Quit()` the loop of both the backing thread *and* the caller/test to
  // give a signal for when the backing thread is terminating, but that task
  // would technically run *during* the backing thread's `Run()` loop. This is a
  // more explicit signal that the internal run loop has ended, and this is the
  // final hook that can run before thread termination.
  OnceClosure backing_thread_terminate_callback_for_testing_;
};

} // namespace base

#endif // BASE_THREADING_THREAD_H_
