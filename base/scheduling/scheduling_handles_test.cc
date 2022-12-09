#include "base/build_config.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_checker.h"
#include "gtest/gtest.h"

// These tests assert that the TaskRunner and TaskLoop handles provided in
// `scheduling_handles.h` work properly between threads.

namespace base {

class SchedulingHandlesTest : public testing::Test {
 public:
  SchedulingHandlesTest() : io_thread_(ThreadType::IO) {}

  virtual void SetUp() override {
    ui_task_loop = TaskLoop::Create(ThreadType::UI);
    io_thread_.Start();
    WaitUntilIOThreadIsRunning();
  }

  virtual void TearDown() override {
    ui_task_loop.reset();
  }

  void WaitUntilIOThreadIsRunning() {
    std::shared_ptr<TaskRunner> io_task_runner = io_thread_.GetTaskRunner();
    io_task_runner->PostTask(ui_task_loop->QuitClosure());
    ui_task_loop->Run();
  }

  void RunOnUIThread() {
    CHECK_ON_THREAD(ThreadType::UI);
  }

  void RunOnIOThread() {
    CHECK_ON_THREAD(ThreadType::IO);

    // UI.
    EXPECT_TRUE(GetUIThreadTaskLoop());
    EXPECT_EQ(GetUIThreadTaskLoop(), ui_task_loop);

    // IO.
    EXPECT_TRUE(GetIOThreadTaskLoop());

    // Current thread.
    EXPECT_TRUE(GetCurrentThreadTaskLoop());
    EXPECT_EQ(GetCurrentThreadTaskLoop(), GetIOThreadTaskLoop());
    EXPECT_TRUE(GetCurrentThreadTaskRunner());

    ui_task_loop->Quit();
  }

  std::shared_ptr<TaskLoop> ui_task_loop;

 private:
  Thread io_thread_;
};

TEST(NoFixture, MainThreadWithNoTaskLoopHasNoSchedulingHandles) {
  EXPECT_FALSE(GetUIThreadTaskLoop());
  EXPECT_FALSE(GetIOThreadTaskLoop());
  EXPECT_FALSE(GetCurrentThreadTaskLoop());
  EXPECT_FALSE(GetCurrentThreadTaskRunner());
}

TEST(NoFixuture, MainThreadWithTaskLoopHasSchedulingHandles) {
  std::shared_ptr<TaskLoop> ui_task_loop = TaskLoop::Create(ThreadType::UI);

  // UI.
  EXPECT_TRUE(GetUIThreadTaskLoop());
  EXPECT_EQ(GetUIThreadTaskLoop(), ui_task_loop);

  // IO.
  EXPECT_FALSE(GetIOThreadTaskLoop());

  // Current thread.
  EXPECT_TRUE(GetCurrentThreadTaskLoop());
  EXPECT_EQ(GetCurrentThreadTaskLoop(), ui_task_loop);
  EXPECT_TRUE(GetCurrentThreadTaskRunner());
}

TEST(NoFixuture, MainThreadWhenTaskLoopShutsDownNoSchedulingHandles) {
  {
    std::shared_ptr<TaskLoop> ui_task_loop = TaskLoop::Create(ThreadType::UI);

    // UI.
    EXPECT_TRUE(GetUIThreadTaskLoop());
    EXPECT_EQ(GetUIThreadTaskLoop(), ui_task_loop);

    // IO.
    EXPECT_FALSE(GetIOThreadTaskLoop());

    // Current thread.
    EXPECT_TRUE(GetCurrentThreadTaskLoop());
    EXPECT_EQ(GetCurrentThreadTaskLoop(), ui_task_loop);
    EXPECT_TRUE(GetCurrentThreadTaskRunner());
  }

  EXPECT_FALSE(GetUIThreadTaskLoop());
  EXPECT_FALSE(GetIOThreadTaskLoop());
  EXPECT_FALSE(GetCurrentThreadTaskLoop());
  EXPECT_FALSE(GetCurrentThreadTaskRunner());
}

// The rest of the tests are actual fixtures of the `SchedulingHandlesTest`
// class.

TEST_F(SchedulingHandlesTest, MainThreadUIandIOHandlesWork) {
  // UI.
  EXPECT_TRUE(GetUIThreadTaskLoop());
  EXPECT_EQ(GetUIThreadTaskLoop(), ui_task_loop);

  // IO.
  EXPECT_TRUE(GetIOThreadTaskLoop());

  // Current thread.
  EXPECT_TRUE(GetCurrentThreadTaskLoop());
  EXPECT_EQ(GetCurrentThreadTaskLoop(), ui_task_loop);
  EXPECT_TRUE(GetCurrentThreadTaskRunner());

  GetCurrentThreadTaskRunner()->PostTask(
    BindOnce(&SchedulingHandlesTest::RunOnUIThread, this)
  );
  ui_task_loop->RunUntilIdle();

  GetIOThreadTaskLoop()->GetTaskRunner()->PostTask(
    BindOnce(&SchedulingHandlesTest::RunOnIOThread, this)
  );
  ui_task_loop->Run(); // The above will automatically quit the loop.
}

}; // namespace base
