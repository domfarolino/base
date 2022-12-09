#include <memory>

#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "gtest/gtest.h"

namespace base {

class ThreadCheckerTest: public testing::Test {
 public:
  ThreadCheckerTest() = default;

  virtual void SetUp() override {
    task_loop_ = TaskLoop::Create(ThreadType::UI);
    worker_1_.Start();
    worker_2_.Start();
    WaitForAllThreadsToStart();
  }

  void WaitForAllThreadsToStart() {
    GetWorker1TaskRunner()->PostTask(task_loop_->QuitClosure());
    task_loop_->Run();
    GetWorker2TaskRunner()->PostTask(task_loop_->QuitClosure());
    task_loop_->Run();
  }

  virtual void TearDown() override {
    printf("TearDown() ------\n");
    task_loop_.reset();
    worker_1_.Stop();
    worker_2_.Stop();
  }

  std::shared_ptr<TaskRunner> GetWorker1TaskRunner() {
    return worker_1_.GetTaskRunner();
  }

  std::shared_ptr<TaskRunner> GetWorker2TaskRunner() {
    return worker_2_.GetTaskRunner();
  }

  void UnblockMainThread() {
    return task_loop_->Quit();
  }

  void Wait() {
    task_loop_->Run();
  }

 private:
  std::shared_ptr<TaskLoop> task_loop_;
  base::Thread worker_1_;
  base::Thread worker_2_;
};

TEST_F(ThreadCheckerTest, ConstructedOnMainThread) {
  std::unique_ptr<ThreadChecker> checker(new ThreadChecker());
  EXPECT_TRUE(checker->CalledOnConstructedThread());

  GetWorker1TaskRunner()->PostTask([&](){
    EXPECT_FALSE(checker->CalledOnConstructedThread());
    UnblockMainThread();
  });
  Wait();

  GetWorker2TaskRunner()->PostTask([&](){
    EXPECT_FALSE(checker->CalledOnConstructedThread());
    UnblockMainThread();
  });
  Wait();
}

TEST_F(ThreadCheckerTest, ConstructedOnWorkerThread) {
  std::unique_ptr<ThreadChecker> checker;

  GetWorker1TaskRunner()->PostTask([&](){
    checker = std::unique_ptr<ThreadChecker>(new ThreadChecker());
    EXPECT_TRUE(checker->CalledOnConstructedThread());
    UnblockMainThread();
  });
  Wait();

  EXPECT_FALSE(checker->CalledOnConstructedThread());

  GetWorker2TaskRunner()->PostTask([&](){
    EXPECT_FALSE(checker->CalledOnConstructedThread());
    UnblockMainThread();
  });
  Wait();
}

}; // namespace base
