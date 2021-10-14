//
// Created by changyuli on 10/13/21.
//

#ifndef SYLAR_SYLAR_THREAD_H_
#define SYLAR_SYLAR_THREAD_H_

// pthread
// std::thread -> pthread
#include "log.h"
#include "util.h"

#include <pthread.h>
#include <semaphore.h>

#include <thread>
#include <functional>
#include <memory>
#include <string>

namespace sylar {

class Semaphore {
 public:
  Semaphore(uint32_t count = 0);
  ~Semaphore();

  void wait();
  void notify();

  Semaphore(const Semaphore&) = delete;
  Semaphore(Semaphore&&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

 private:
  sem_t m_semaphore;
};

template <typename T>
struct ScopedLockImpl {
 public:
  explicit ScopedLockImpl(T& mutex) : m_mutex(mutex) {
    m_mutex.lock();
    m_locked = true;
  }

  ~ScopedLockImpl() {
    unlock();
  }

  void lock() {
    if (!m_locked) {
      m_mutex.lock();
      m_locked = true;
    }
  }

  void unlock() {
    if (m_locked) {
      m_mutex.unlock();
      m_locked = false;
    }
  }

 private:
  T& m_mutex;
  bool m_locked;
};

class Mutex {
 public:

  using Lock = ScopedLockImpl<Mutex>;

  Mutex() {
	pthread_mutex_init(&m_mutex, nullptr);
  }

  ~Mutex() {
	pthread_mutex_destroy(&m_mutex);
  }

  void lock() {
	pthread_mutex_lock(&m_mutex);
  }

  void unlock() {
	pthread_mutex_unlock(&m_mutex);
  }

 private:
  pthread_mutex_t m_mutex;
};

template<typename T>
struct ReadScopedLockImpl {
 public:
  explicit ReadScopedLockImpl(T& mutex) : m_mutex(mutex) {
    m_mutex.rdlock();
    m_locked = true;
  }

  ~ReadScopedLockImpl() {
    unlock();
  }

  void lock() {
	if (!m_locked) {
	  m_mutex.rdlock();
	  m_locked = true;
	}
  }

  void unlock() {
	if (m_locked) {
	  m_mutex.unlock();
	  m_locked = false;
	}
  }

 private:
  T& m_mutex;
  bool m_locked;
};

template<typename T>
struct WriteScopedLockImpl {
 public:
  explicit WriteScopedLockImpl(T& mutex) : m_mutex(mutex) {
	m_mutex.wrlock();
	m_locked = true;
  }

  ~WriteScopedLockImpl() {
	unlock();
  }

  void lock() {
	if (!m_locked) {
	  m_mutex.rdlock();
	  m_locked = true;
	}
  }

  void unlock() {
	if (m_locked) {
	  m_mutex.unlock();
	  m_locked = false;
	}
  }

 private:
  T& m_mutex;
  bool m_locked;
};

class RWMutex {
 public:

  using ReadLock = ReadScopedLockImpl<RWMutex>;
  using WriteLock = WriteScopedLockImpl<RWMutex>;

  RWMutex() {
	pthread_rwlock_init(&m_lock, nullptr);
  }

  ~RWMutex() {
	pthread_rwlock_destroy(&m_lock);
  }

  void rdlock() {
	pthread_rwlock_rdlock(&m_lock);
  }

  void wrlock() {
	pthread_rwlock_wrlock(&m_lock);
  }

  void unlock() {
	pthread_rwlock_unlock(&m_lock);
  }

 private:
  pthread_rwlock_t m_lock;
};

class Thread {
 public:
  using ptr = std::shared_ptr<Thread>;
  Thread(std::function<void()> cb, const std::string& name);
  ~Thread();

  pid_t getId() const {return m_id;}
  const std::string& getName() const {return m_name;}
  void join();

  static Thread* GetThis();
  static const std::string& GetName();
  static void SetName(const std::string& name);

  Thread(const Thread&) = delete;
  Thread(Thread&&) = default;
  Thread& operator=(const Thread&) = delete;

 private:
  static void* run(void*);

 private:
  pid_t m_id = -1;
  pthread_t m_thread = 0;
  std::function<void()> m_cb;
  std::string m_name;

  Semaphore m_semaphore;
};

}

#endif //SYLAR_SYLAR_THREAD_H_
