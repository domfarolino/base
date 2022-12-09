#include "gtest/gtest.h"

#include "base/build_config.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"
#include "base/threading/simple_thread.h"

// These tests are general TaskLoop tests that should pass for all kinds of task
// loops regardless of |base::ThreadType|.

namespace base {

class TaskLoopTest : public testing::Test,
                     public testing::WithParamInterface<ThreadType> {
 public:
  // Provides meaningful param names instead of /0 and /1 etc.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case base::ThreadType::UI:
        return "UI";
      case base::ThreadType::IO:
        return "IO";
      case base::ThreadType::WORKER:
        return "WORKER";
    }

    NOTREACHED();
    return "NOTREACHED";
  }

  TaskLoopTest() : thread_type_(GetParam()) {}

  virtual void SetUp() override {
    task_loop = TaskLoop::Create(thread_type_);
  }

  virtual void TearDown() override {
    task_loop.reset();
  }

  std::shared_ptr<TaskLoop> task_loop;

 private:
  base::ThreadType thread_type_;
};

//////////      These tests only use |TaskLoop::Run()|      //////////
///// RunUntilIdle() and QuitWhenIdle() are explicitly tested further down /////

TEST_P(TaskLoopTest, QuitBeforeRun) {
  task_loop->Quit();
  task_loop->Run(); // Loop should immediately quit. Test should not time out.
}

// The first that that an already-quit `TaskLoop` should do upon `Run()` is
// immediately quit. Tasks posted to it should not run.
TEST_P(TaskLoopTest, PostTaskAndQuitBeforeRun) {
  bool task_ran = false;
  task_loop->PostTask([&](){
    task_ran = true;
  });
  task_loop->Quit();
  task_loop->Run(); // Loop should immediately quit. Test should not time out.
  EXPECT_FALSE(task_ran);
}

// This test verifies that multiple `Quit()` calls are idempotent. Above, we
// already test that if you call `Quit()`, `Run()` will immediately exit, but
// this test ensures that if you call `Quit()` multiple times when before
// running the loop, the first `Run()` immediately quit but the next one will
// run the loop and process any queued tasks.
TEST_P(TaskLoopTest, PostTaskAndQuitQuitBeforeRun) {
  bool task_ran = false;
  task_loop->PostTask([&](){
    task_ran = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  });
  task_loop->Quit();
  task_loop->Quit();

  task_loop->Run();
  EXPECT_FALSE(task_ran);
  task_loop->Run();
  EXPECT_TRUE(task_ran);
}

// This test is the same as the above one in that we call `Quit()` `Quit()`
// `Run()` `Run()` in that order; the only difference is that we don't ever post
// a task. We need to test that the second `Run()` call doesn't crash the
// `TaskLoop` specifically when there is no task already posted to the loop —
// this is a regression test for a bug found in `TaskLoopForIO`. The correct
// behavior is that the second `Run()` call will just run the loop as normal.
// But since we need a way to quit the test after the loop is running like
// normal, we have to have a manual timeout that we run on a separate worker
// thread. Once the manual timeout is up, we kill the main `TaskLoop`, and as
// long as the test did not blow up before that, we pass.
TEST_P(TaskLoopTest, QuitQuitRunRun) {
  SimpleThread thread([&](){
    base::Thread::sleep_for(std::chrono::milliseconds(300));
    task_loop->Quit();
  });

  task_loop->Quit();
  task_loop->Quit();
  task_loop->Run();
  // This should not crash (specifically when there is no pending task posted at
  // this point). As long as this doesn't crash, the timeout above in `thread`
  // will kill appropriately quit `task_loop` once it is running like normal.
  task_loop->Run();
}

TEST_P(TaskLoopTest, PostTasksBeforeRun) {
  // When the loop is |Run()|, it immediately runs all posted tasks. We assert
  // that the first task is run by observing its side effects, and we assert
  // that the last task runs because the loop quits and the test does not time
  // out.
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });
  task_loop->PostTask(BindOnce([](OnceClosure quit_closure){
    quit_closure();
  }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(first_task_ran, true);
}

TEST_P(TaskLoopTest, RunQuitRunQuit) {
  bool first_task_ran = false;
  task_loop->PostTask(BindOnce([&](OnceClosure quit_closure){
    first_task_ran = true;
    quit_closure();
  }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(first_task_ran, true);

  bool second_task_ran = false;
  task_loop->PostTask(BindOnce([&](OnceClosure quit_closure){
    second_task_ran = true;
    quit_closure();
  }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, NestedTasks) {
  bool outer_task_ran = false;
  bool inner_task_ran = false;

  task_loop->PostTask([&](){
    outer_task_ran = true;
    task_loop->PostTask([&](){
      inner_task_ran = true;
      task_loop->Quit();
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->Run();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(inner_task_ran, true);
}

TEST_P(TaskLoopTest, GetCurrentThreadTaskRunner) {
  bool first_task_ran = false;
  base::GetCurrentThreadTaskRunner()->PostTask(
    BindOnce([&](OnceClosure quit_closure){
      first_task_ran = true;
      quit_closure();
    }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(first_task_ran, true);
}

TEST_P(TaskLoopTest, NestedGetCurrentThreadTaskRunner) {
  bool outer_task_ran = false;
  bool inner_task_ran = false;
  base::GetCurrentThreadTaskRunner()->PostTask([&](){
    outer_task_ran = true;
    base::GetCurrentThreadTaskRunner()->PostTask([&](){
      inner_task_ran = true;
      task_loop->Quit();
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->Run();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(inner_task_ran, true);
}

///////////////// These tests exercise |TaskLoop::RunUntilIdle()| and
////////////////  |TaskLoop::QuitWhenIdle()|

TEST_P(TaskLoopTest, RunUntilIdleImmediatelyQuits) {
  // Loop should immediately quit. Test should not time out.
  task_loop->RunUntilIdle();
}

TEST_P(TaskLoopTest, QuitBeforeRunUntilIdle) {
  task_loop->Quit();
  // Loop should immediately quit. Test should not time out.
  task_loop->RunUntilIdle();
}

TEST_P(TaskLoopTest, QuitWhenIdleImmediatelyQuits) {
  task_loop->QuitWhenIdle();
  task_loop->Run();
}

TEST_P(TaskLoopTest, PostTasksBeforeRunUntilIdle) {
  // When the loop is |RunUntilIdle()|, it immediately runs all posted tasks. We
  // assert that the first task is run by observing its side effects, and we
  // assert that |RunUntilIdle()| actually works by the test immediately ending
  // after the task is run and not timing out.
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });
  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleQuitRunUntilIdleQuit) {
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);

  bool second_task_ran = false;
  task_loop->PostTask([&](){
    second_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleMultipleTasks) {
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });

  bool second_task_ran = false;
  task_loop->PostTask([&](){
    second_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleEarlyQuit) {
  bool first_task_ran = false;
  task_loop->PostTask(
    BindOnce([&](OnceClosure quit_closure){
      first_task_ran = true;
      quit_closure();
    }, task_loop->QuitClosure()));


  bool second_task_ran = false;
  task_loop->PostTask([&](){
    second_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);
  EXPECT_EQ(second_task_ran, false);

  task_loop->RunUntilIdle();
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleDoesNotSnapshotTheEventQueueSize) {
  bool outer_task_ran = false;
  bool continuation_task_ran = false;

  task_loop->PostTask([&](){
    outer_task_ran = true;
    task_loop->PostTask([&](){
      continuation_task_ran = true;
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->RunUntilIdle();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(continuation_task_ran, true);
}

// Tests that even though the QuitWhenIdle() signal was sent first,
// recently-added work before Run() is still processed. The loop only quits
// when it is truly idle.
TEST_P(TaskLoopTest, QuitWhenIdleBeforeRun) {
  task_loop->QuitWhenIdle();

  bool outer_task_ran = false;
  bool continuation_task_ran = false;

  task_loop->PostTask([&](){
    outer_task_ran = true;
    task_loop->PostTask([&](){
      continuation_task_ran = true;
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->Run();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(continuation_task_ran, true);
}

TEST_P(TaskLoopTest, QuitWhenIdleMidTask) {
  bool outer_task_ran = false;
  bool continuation_task_ran = false;

  task_loop->PostTask([&](){
    outer_task_ran = true;
    task_loop->QuitWhenIdle();

    task_loop->PostTask([&](){
      continuation_task_ran = true;
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->Run();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(continuation_task_ran, true);
}


INSTANTIATE_TEST_SUITE_P(All,
                         TaskLoopTest,
                         testing::Values(ThreadType::UI, ThreadType::IO, ThreadType::WORKER),
                         &TaskLoopTest::DescribeParams);

}; // namespace base
