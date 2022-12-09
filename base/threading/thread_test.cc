#include <memory>

#include "base/build_config.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/thread.h"
#include "gtest/gtest.h"

namespace base {

class ThreadTest : public testing::Test,
                   public testing::WithParamInterface<ThreadType> {
 public:
  ThreadTest() = default;

  // Provides meaningful param names instead of /0 and /1 etc.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case ThreadType::UI:
        return "UI";
      case ThreadType::IO:
        return "IO";
      case ThreadType::WORKER:
        return "WORKER";
    }

    NOTREACHED();
    return "NOTREACHED";
  }

  void SetUp() override {
    CHECK(!thread);
    thread = std::unique_ptr<Thread>(new Thread(GetParam()));
  }

  void TearDown() override {
    thread.reset();
  }

 protected:
  std::unique_ptr<Thread> thread;
};

// These two tests are regression tests for
// https://github.com/domfarolino/browser/issues/40.
TEST_P(ThreadTest, ImplicitDestroyRaceConditionOnThreadDelegate) {
  thread->Start();
}
TEST_P(ThreadTest, ImplicitDestroyRaceConditionOnThreadDelegateConditionVariable) {
  thread->Start();
  thread->GetTaskRunner()->PostTask([](){});
}

// Death test.
// The documentation [1] strongly recommends using the "*DeathTest" suffix for
// naming, however I can not get this to work with parameterized tests for some
// reason. Everything appears to be fine without the suffix though.
// [1]: https://github.com/google/googletest/blob/master/docs/advanced.md#death-test-naming
TEST_P(ThreadTest, StartStart) {
  thread->Start();
  ASSERT_DEATH({ thread->Start(); }, "!started_");
}

TEST_P(ThreadTest, StartStopStop) {
  thread->Start();
  thread->Stop();
  thread->Stop();
}

TEST_P(ThreadTest, StopStopWithoutStart) {
  thread->Stop();
  thread->Stop();
}

TEST_P(ThreadTest, StartStopStartStop) {
  thread->Start();
  thread->Stop();
  thread->Start();
  thread->Stop();
}

TEST_P(ThreadTest, GetTaskRunnerBeforeStart) {
  EXPECT_FALSE(thread->GetTaskRunner());
  ASSERT_DEATH({ thread->GetTaskRunner()->PostTask([](){}); }, "");
}

TEST_P(ThreadTest, StartAfterThreadTerminatesButBeforeJoinOrStop) {
  std::shared_ptr<TaskLoop> delegate_reset_waiter =
    TaskLoop::Create(ThreadType::WORKER);
  thread->RegisterDelegateResetCallbackForTesting(
    delegate_reset_waiter->QuitClosure());

  thread->Start();
  thread->GetTaskRunner()->PostTask([](){
    base::GetCurrentThreadTaskLoop()->Quit();
  });

  // We'll run / "wait" until we get confirmation that the delegate has been
  // reset.
  delegate_reset_waiter->Run();

  // At this point we know that the thread's delegate has been "quit" and the
  // underlying thread has been entirely terminated. Delegate should be reset
  // and therefore unable to produce task runners.
  EXPECT_FALSE(thread->GetTaskRunner());

  // Calling |Start()| should fail because neither |Stop()| nor |join()| have
  // been called yet.
  ASSERT_DEATH({ thread->Start(); }, "!started_");
}

TEST_P(ThreadTest, StartJoinStartJoin) {
  thread->Start();

  bool first_task_ran = false;
  thread->GetTaskRunner()->PostTask([&](){
    first_task_ran = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  });
  thread->join();
  EXPECT_TRUE(first_task_ran);

  thread->Start();

  bool second_task_ran = false;
  thread->GetTaskRunner()->PostTask([&](){
    second_task_ran = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  });
  thread->join();
  EXPECT_TRUE(second_task_ran);
}

TEST_P(ThreadTest, StopBeforeFirstStartHasNoEffect) {
  thread->Stop();

  thread->Start();
  thread->GetTaskRunner()->PostTask([&](){
    base::GetCurrentThreadTaskLoop()->Quit();
  });

  thread->join();
  // This test will only finish (and thus not timeout) if the task we posted
  // above is called. That task is called because the Stop() before the first
  // Start() has no effect. If Stop()s before the first Start() immediately
  // stopped the thread once it was started, then our task would never run, the
  // thread would never stop, join() would hang, and this test would timeout.
}

// This test ensures that |base::Thread::Stop()| is idempotent; that is, if you
// call it multiple times and then post a real task, you don't have to call
// |base::Thread::Start()| as many times just to finally get to the next task.
// The |Stop()|s are not queued.
TEST_P(ThreadTest, StopIsIdempotent) {
  thread->Start();

  thread->Stop();
  thread->Stop();
  thread->Stop();
  thread->Stop();
  thread->Stop();
  thread->Stop();

  thread->Start();
  bool task_ran = false;
  thread->GetTaskRunner()->PostTask([&](){
    task_ran = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  });

  thread->join();
  EXPECT_TRUE(task_ran);
}

TEST_P(ThreadTest, StopDoesNotRunQueuedTasks) {
  bool second_task_ran = false;
  std::shared_ptr<TaskLoop> waiter = TaskLoop::Create(ThreadType::WORKER);

  thread->Start();

  thread->GetTaskRunner()->PostTask([&](){
    waiter->Quit();

    base::GetCurrentThreadTaskLoop()->PostTask([&]() {
      second_task_ran = true;
    });

    // Synchronously sleep for a while after we posted the second task. This
    // will give the "main" thread a chance to send the Quit() signal to *this*
    // thread's TaskLoop, which should quit before running the second task that
    // we just posted.
    base::Thread::sleep_for(std::chrono::milliseconds(200));
  });

  waiter->Run();
  thread->Stop();
  EXPECT_FALSE(second_task_ran);
}

TEST_P(ThreadTest, StopWhenIdleRunsQueuedTasks) {
  bool second_task_ran = false;
  std::shared_ptr<TaskLoop> waiter = TaskLoop::Create(ThreadType::WORKER);

  thread->Start();

  thread->GetTaskRunner()->PostTask([&](){
    waiter->Quit();

    base::GetCurrentThreadTaskLoop()->PostTask([&]() {
      second_task_ran = true;
    });

    // Synchronously sleep for a while after we posted the second task. This
    // will give the "main" thread a chance to send the Quit() signal to *this*
    // thread's TaskLoop, which should quit before running the second task that
    // we just posted.
    base::Thread::sleep_for(std::chrono::milliseconds(200));
  });

  waiter->Run();
  thread->StopWhenIdle();
  EXPECT_TRUE(second_task_ran);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ThreadTest,
                         testing::Values(ThreadType::UI, ThreadType::IO, ThreadType::WORKER),
                         &ThreadTest::DescribeParams);

}; // namespace base
