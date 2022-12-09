#include <assert.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/threading/simple_thread.h"
#include "gtest/gtest.h"

namespace base {

// These are messages that will be written to an underlying buffer via a file
// descriptor. For simplicity, their length should all be |kMessageLength|.
static const std::string kFirstMessage  =     "First message ";
static const std::string kSecondMessage =     "Second message";
static const std::string kThirdMessage  =     "Third message ";
static const std::string kFourthMessage =     "Fourth message";
static const int kMessageLength = 14;

// With this test base class, all tests and assertions will be running on a
// thread on which a |TaskLoopForIO| is bound. The tests may create and use
// other threads to asynchronously interact with the IO task loop on the main
// thread. In these cases, it will be useful for the tests to call
// |task_loop_for_io->Run()| to wait for tasks and events to be posted.
class TaskLoopForIOTestBase : public testing::Test {
 public:
  void SetUp() {
    task_loop_for_io = std::static_pointer_cast<base::TaskLoopForIO>(
                          base::TaskLoop::Create(base::ThreadType::IO));

    // Setup and entangle two file descriptors to/from which tests can read and
    // write.
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    EXPECT_EQ(fcntl(fds[0], F_SETFL, O_NONBLOCK), 0);
    EXPECT_EQ(fcntl(fds[1], F_SETFL, O_NONBLOCK), 0);

    expected_message_count_ = 0;
    messages_read.clear();
  }

  void TearDown() {
    task_loop_for_io.reset();

    EXPECT_EQ(close(fds[0]), 0);
    EXPECT_EQ(close(fds[1]), 0);
  }

  std::shared_ptr<base::TaskRunner> GetTaskRunner() {
    return task_loop_for_io->GetTaskRunner();
  }

  void OnMessageRead(std::string message) {
    messages_read.push_back(message);
    if ((int)messages_read.size() == expected_message_count_)
      task_loop_for_io->Quit();
  }

  void SetExpectedMessageCount(int count) {
    expected_message_count_ = count;
  }

  int fds[2];
  std::shared_ptr<base::TaskLoopForIO> task_loop_for_io;
  std::vector<std::string> messages_read;

 private:
  int expected_message_count_;
};

class TestSocketReader : public base::TaskLoopForIO::SocketReader {
 public:
  TestSocketReader(int fd, TaskLoopForIOTestBase& callback_object) :
    base::TaskLoopForIO::SocketReader::SocketReader(fd),
    callback_object_(callback_object) {
    std::static_pointer_cast<base::TaskLoopForIO>(
      base::GetIOThreadTaskLoop())->WatchSocket(this);
  }
  ~TestSocketReader() {
    std::static_pointer_cast<base::TaskLoopForIO>(
      base::GetIOThreadTaskLoop())->UnwatchSocket(this);
  }

  void OnCanReadFromSocket() override {
    char buffer[kMessageLength];
    struct iovec iov = {buffer, kMessageLength};
    char cmsg_buf[CMSG_SPACE(kMessageLength)];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    std::string message(buffer, buffer + kMessageLength);
    callback_object_.OnMessageRead(message);
  }

 private:
  TaskLoopForIOTestBase& callback_object_;
};

// This should be used directly when you want to write a message from another
// thread, and be notified on the main thread (via |callback|) when the write is
// complete.
void WriteMessagesAndInvokeCallback(int fd, std::vector<std::string> messages,
                                    OnceClosure callback) {
  for (const std::string& message : messages) {
    write(fd, message.data(), message.size());
  }

  callback();
}

void WriteMessages(int fd, std::vector<std::string> messages) {
  WriteMessagesAndInvokeCallback(fd, messages, [](){});
}

TEST_F(TaskLoopForIOTestBase, BasicSocketReading) {
  SetExpectedMessageCount(1);

  std::unique_ptr<TestSocketReader> reader(new TestSocketReader(fds[0], *this));

  std::vector<std::string> messages_to_write = {kFirstMessage};
  base::SimpleThread simple_thread(WriteMessages, fds[1], messages_to_write);
  task_loop_for_io->Run();
  EXPECT_EQ(messages_read.size(), 1);
  EXPECT_EQ(messages_read[0], kFirstMessage);
}

// Just like the test above, but we write to the socket before the task loop is
// actually aware of the file descriptor to listen from. We're testing that we
// can read writes that were queued before we started listening.
TEST_F(TaskLoopForIOTestBase, WriteToSocketBeforeListening) {
  SetExpectedMessageCount(1);

  std::vector<std::string> messages = {kFirstMessage};
  base::SimpleThread simple_thread(WriteMessagesAndInvokeCallback, fds[1],
                                   messages,
                                   task_loop_for_io->QuitClosure());

  // This will make us wait until the message above has been written. Once
  // written, the test will continue.
  task_loop_for_io->Run();

  std::unique_ptr<TestSocketReader> reader(new TestSocketReader(fds[0], *this));
  task_loop_for_io->Run();
  EXPECT_EQ(messages_read.size(), 1);
  EXPECT_EQ(messages_read[0], kFirstMessage);
}

TEST_F(TaskLoopForIOTestBase, QueueingMessagesOnMultipleSockets) {
  SetExpectedMessageCount(4);

  // Set up the file descriptors but do not register socket readers yet.
  int moreFds[2];
  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, moreFds), 0);
  EXPECT_EQ(fcntl(moreFds[0], F_SETFL, O_NONBLOCK), 0);
  EXPECT_EQ(fcntl(moreFds[1], F_SETFL, O_NONBLOCK), 0);

  // Two separate threads each queue a message
  std::vector<std::string> messages_1 = {kFirstMessage, kSecondMessage};
  std::vector<std::string> messages_2 = {kThirdMessage, kFourthMessage};
  base::SimpleThread thread_1(WriteMessagesAndInvokeCallback, fds[1],
                              messages_1, task_loop_for_io->QuitClosure());
  task_loop_for_io->Run(); // Will resume once the first two messages are written.
  base::SimpleThread thread_2(WriteMessagesAndInvokeCallback, moreFds[1],
                              messages_2, task_loop_for_io->QuitClosure());
  task_loop_for_io->Run(); // Will resume once the last two messages are written.

  // At this point the writes have been queued by the OS, but not exposed to the
  // loop's via the kernel.
  EXPECT_EQ(messages_read.size(), 0);

  // Create and register SocketReaders to listen to the above messages.
  std::unique_ptr<TestSocketReader> reader_1(new TestSocketReader(fds[0], *this));
  std::unique_ptr<TestSocketReader> reader_2(new TestSocketReader(moreFds[0], *this));

  task_loop_for_io->Run(); // Process the queued messages.
  EXPECT_EQ(messages_read.size(), 4);

  // Close out reader while its file descriptor is still valid.
  reader_2.reset();

  // The ordering looks funky here because our task loop services each file
  // descriptor evenly, instead of starving one of them by reading all of the
  // other's events.
  EXPECT_EQ(messages_read[0], kFirstMessage);
  EXPECT_EQ(messages_read[1], kThirdMessage);
  EXPECT_EQ(messages_read[2], kSecondMessage);
  EXPECT_EQ(messages_read[3], kFourthMessage);

  EXPECT_EQ(close(moreFds[0]), 0);
  EXPECT_EQ(close(moreFds[1]), 0);
}

TEST_F(TaskLoopForIOTestBase, InterleaveTaskAndMessages) {
  int num_tasks = 0;

  // Set up the SocketReader.
  std::unique_ptr<TestSocketReader> reader(new TestSocketReader(fds[0], *this));

  // Write the first message & post the first task.
  WriteMessages(fds[1], {kFirstMessage});
  task_loop_for_io->PostTask([&]() {
    EXPECT_EQ(num_tasks, 0);
    num_tasks++;
    EXPECT_EQ(messages_read.size(), 1);
    EXPECT_EQ(messages_read[0], kFirstMessage);
  });

  // Write the second message & post the first task.
  WriteMessages(fds[1], {kSecondMessage});
  task_loop_for_io->PostTask([&]() {
    EXPECT_EQ(num_tasks, 1);
    num_tasks++;
    EXPECT_EQ(messages_read.size(), 2);
    EXPECT_EQ(messages_read[1], kSecondMessage);
  });

  // Write the third message & post the first task.
  WriteMessages(fds[1], {kThirdMessage});
  task_loop_for_io->PostTask([&]() {
    EXPECT_EQ(num_tasks, 2);
    num_tasks++;
    EXPECT_EQ(messages_read.size(), 3);
    EXPECT_EQ(messages_read[1], kThirdMessage);
    task_loop_for_io->Quit();
  });
}

TEST_F(TaskLoopForIOTestBase, QuitWhenIdleStillProcessesReads) {
  SetExpectedMessageCount(1);

  // First we write the message and confirm that it has been processed.
  std::vector<std::string> messages = {kFirstMessage};
  base::SimpleThread simple_thread(WriteMessagesAndInvokeCallback, fds[1],
                                   messages,
                                   task_loop_for_io->QuitClosure());

  // This will make us wait until the message above has been written. Once
  // written, the test will continue.
  task_loop_for_io->Run();

  // Tell the loop to quit when it is idle. This will take effect the next time
  // it is run.
  task_loop_for_io->QuitWhenIdle();
  std::unique_ptr<TestSocketReader> reader(new TestSocketReader(fds[0], *this));
  task_loop_for_io->Run();
  EXPECT_EQ(messages_read.size(), 1);
  EXPECT_EQ(messages_read[0], kFirstMessage);
}

// Almost identical to the previous test, except |QuitWhenIdle()| is called
// *after* the SocketReader registers itself with |TaskLoopForIO|, as a part of
// |RunUntilIdle()|.
TEST_F(TaskLoopForIOTestBase, RunUntilIdleStillProcessesReads) {
  SetExpectedMessageCount(1);

  // First we write the message and confirm that it has been processed.
  std::vector<std::string> messages = {kFirstMessage};
  base::SimpleThread simple_thread(WriteMessagesAndInvokeCallback, fds[1],
                                   messages,
                                   task_loop_for_io->QuitClosure());

  // This will make us wait until the message above has been written. Once
  // written, the test will continue.
  task_loop_for_io->Run();

  // Tell the loop to quit when it is idle. This will take effect the next time
  // it is run.
  std::unique_ptr<TestSocketReader> reader(new TestSocketReader(fds[0], *this));
  task_loop_for_io->RunUntilIdle();
  EXPECT_EQ(messages_read.size(), 1);
  EXPECT_EQ(messages_read[0], kFirstMessage);
}

}; // namespace base
