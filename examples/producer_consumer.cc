#include <assert.h>

#include <iostream>
#include <cstdlib> // rand()
#include <queue>

#include "base/check.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"
#include "base/threading/simple_thread.h"

// Returns a random wait period in ms, weighted to return lower milliesconds
// more frequently.
int get_random_wait() {
  int random = rand() % 10 + 1; // [1, 10]
  if (random <= 4) random = 0; // [1, 4]
  else if (random <= 6) random = 20; // [5, 6]
  else if (random <= 8) random = 70; // [7, 8]
  else if (random <= 9) random = 100; // [9, 9]
  else if (random == 10) random = 200; // [10, 10]
  return random;
}

// This program demonstrates a simplified version of the classic
// producer-consumer problem with threads synchronizing a shared resource,
// in this case, a message std::queue.

void producer(std::queue<std::string>& q, base::Mutex& mutex,
              base::ConditionVariable& condition) {
  // Guaranteed to produce at least one string.
  int num_strings_to_produce = rand() % 100 + 1, i = 0;

  std::string tmp;
  while (num_strings_to_produce) {
    mutex.lock();

    // If this is our last string, make it the "quit" string.
    if (num_strings_to_produce == 1) {
      q.push("quit");
      std::cout << "\x1B[33m Producer producing quit message\x1b[00m" << std::endl;
    } else {
      tmp = "string" + std::to_string(i++);
      q.push(tmp);
      std::cout << "\x1B[33m Producer producing '" << tmp << "'\x1b[00m" << std::endl;
    }
    mutex.unlock();
    condition.notify_one();

    // Delay the producer's message by random time.
    base::Thread::sleep_for(std::chrono::milliseconds(get_random_wait()));
    num_strings_to_produce--;
  }
}

void consumer(std::queue<std::string>& q, base::Mutex& mutex,
              base::ConditionVariable& condition) {
  std::string data = "";
  while (data != "quit") {
    // Optionally wait, if the predicate function returns false.
    condition.wait(mutex, [&]() -> bool{
      bool can_skip_waiting = (q.empty() == false);
      if (can_skip_waiting == false)
        std::cout << "\x1B[34m   Consumer waiting for more input\x1B[00m" << std::endl;
      return can_skip_waiting;
    });

    CHECK(mutex.is_locked());

    data = q.front();
    std::cout << "\x1B[32m Consumer consuming '" << data << "'\x1B[00m" << std::endl;
    q.pop();
    condition.release_lock();
  }
}

int main() {
  srand(time(NULL));
  base::Mutex mutex(base::ThreadMode::kUsingPthread);
  base::ConditionVariable condition(base::ThreadMode::kUsingPthread);

  std::queue<std::string> message_queue;
  base::SimpleThread producer_thread(producer, std::ref(message_queue), std::ref(mutex), std::ref(condition));
  consumer(message_queue, mutex, condition);
  producer_thread.join();
  return 0;
}
