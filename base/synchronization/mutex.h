#ifndef BASE_SYNCHRONIZATION_MUTEX_H_
#define BASE_SYNCHRONIZATION_MUTEX_H_

#include <pthread.h>

#include <mutex>  // std::unique_lock.

#include "base/check.h"
#include "base/synchronization/synchronization_helpers.h"

namespace base {

class Mutex {
 public:
  Mutex(): Mutex(ThreadMode::kUsingCpp) {}
  explicit Mutex(ThreadMode mode): mode_(mode) {
    CHECK(!pthread_mutex_init(&pthread_mutex_, nullptr));
  }

  Mutex(Mutex&) = delete;
  Mutex(Mutex&&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  operator std::mutex&() {
    CHECK_EQ(mode_, ThreadMode::kUsingCpp);
    return cpp_mutex_;
  }

  pthread_mutex_t& GetAsPthreadMutex() {
    CHECK_EQ(mode_, ThreadMode::kUsingPthread);
    return pthread_mutex_;
  }

  void lock() {
    if (mode_ == ThreadMode::kUsingCpp) {
      cpp_mutex_.lock();
    } else {
      pthread_mutex_lock(&pthread_mutex_);
    }
  }

  void unlock() {
    if (mode_ == ThreadMode::kUsingCpp) {
      cpp_mutex_.unlock();
    } else {
      pthread_mutex_unlock(&pthread_mutex_);
    }
  }

  bool is_locked() {
    if (mode_ == ThreadMode::kUsingCpp) {
      if (cpp_mutex_.try_lock()) {
        unlock();
        return false;
      }

      return true;
    } else {
      if (pthread_mutex_trylock(&pthread_mutex_) == 0) {
        unlock();
        return false;
      }

      return true;
    }
  }

  ~Mutex() {
    pthread_mutex_destroy(&pthread_mutex_);
  }

 private:
  ThreadMode mode_;

  std::mutex cpp_mutex_;
  pthread_mutex_t pthread_mutex_;
};

} // namespace base

#endif // BASE_SYNCHRONIZATION_MUTEX_H_
