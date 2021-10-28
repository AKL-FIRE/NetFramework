//
// Created by changyuli on 10/16/21.
//

#ifndef SYLAR_SYLAR_SCHEDULER_H_
#define SYLAR_SYLAR_SCHEDULER_H_

#include <memory>
#include <list>
#include <utility>

#include "fiber.h"

namespace sylar {

class Scheduler {
 public:
  using ptr = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;

  explicit Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");
  virtual ~Scheduler();

  const std::string& getName() const {return m_name;}

  static Scheduler* GetThis();
  static Fiber* GetMainFiber();

  void start();
  void stop();

  template <typename FiberOrCb>
  void schedule(FiberOrCb fc, int threadId = -1) {
    bool need_tickle = false;
	{
	  MutexType::Lock lock(m_mutex);
	  need_tickle = scheduleNoLock(fc, threadId);
	}
	if (need_tickle) {
	  tickle();
	}
  }

  template <typename InputIterator>
  void schedule(InputIterator begin, InputIterator end) {
    bool need_tickle = false;
	{
	  MutexType::Lock lock(m_mutex);
	  while (begin != end) {
	    need_tickle = scheduleNoLock(&*begin) || need_tickle;
	  }
	}
	if (need_tickle) {
	  tickle();
	}
  }
 protected:
  virtual void tickle();
  void run();
  virtual bool stopping(); // 停止时的清理工作
  virtual void idle(); // 无任务时执行

  void setThis();

  bool hasIdleThreads() {return m_idleThreadCount > 0;}
 private:
  template <typename FiberOrCb>
  bool scheduleNoLock(FiberOrCb fc, int threadId) {
    /*
     * need_tickle为true表示以前没有任何的任务，即0　-> 1
     * */
    bool need_tickle = m_fibers.empty();
    FiberAndThread ft(fc, threadId);
    if (ft.fiber || ft.cb) {
      m_fibers.push_back(ft);
    }
    return need_tickle;
  }

 private:
  struct FiberAndThread {
    Fiber::ptr fiber;
    std::function<void()> cb;
    int threadId;

    FiberAndThread(Fiber::ptr f, int thr)
      : fiber(std::move(f)), threadId(thr) {}

    FiberAndThread(Fiber::ptr* f, int thr)
      : threadId(thr) {
      fiber.swap(*f);
    }

    FiberAndThread(std::function<void()> f, int thr)
      : cb(std::move(f)), threadId(thr) {}

    FiberAndThread(std::function<void()>* f, int thr) : threadId(thr) {
      cb.swap(*f);
    }

    FiberAndThread() : threadId(-1) {}

    void reset() {
      fiber = nullptr;
      cb = nullptr;
      threadId = -1;
    }
  };

 private:
  MutexType m_mutex;
  std::vector<Thread::ptr> m_threads; // 线程池
  std::list<FiberAndThread> m_fibers; // 任务队列
  Fiber::ptr m_rootFiber; // 主协程
  std::string m_name;

 protected:
  std::vector<int> m_threadIds;
  size_t m_threadCount = 0;
  std::atomic<size_t> m_activeThreadCount {0};
  std::atomic<size_t> m_idleThreadCount {0};
  bool m_stopping = true;
  bool m_autoStop = false;
  int m_rootThread = 0; // 主线程Ｉd
};

}

#endif //SYLAR_SYLAR_SCHEDULER_H_
