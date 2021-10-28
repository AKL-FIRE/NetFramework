//
// Created by changyuli on 10/28/21.
//

#ifndef SYLAR_SYLAR_TIMER_H_
#define SYLAR_SYLAR_TIMER_H_

#include "thread.h"

#include <memory>
#include <set>

namespace sylar {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
  friend class TimerManager;
 public:
  using ptr = std::shared_ptr<Timer>;

 private:
  Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);
  struct Comparator {
    bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
  };

 private:
  bool m_recurring = false; // 是否是循环定时器
  uint64_t m_ms = 0;        // 时间间隔(周期)
  uint64_t m_next = 0;      // 精确的执行时间
  std::function<void()> m_cb; // 要执行的任务
  TimerManager* m_manager = nullptr;
};

class TimerManager {
  friend class Timer;
 public:
  using RWMutexType = RWMutex;

  TimerManager();
  virtual ~TimerManager();

  // 获取一个ms毫秒后执行cb的定时器
  Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
  // 获取一个ms毫秒后执行cb(当weak_cond可以提升为shared_ptr时执行，否则不执行)
  Timer::ptr addConditionalTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);

 protected:
  virtual void onTimerInsertAtFront() = 0;
 private:
  RWMutexType m_mutex;
  std::set<Timer::ptr, Timer::Comparator> m_timers;
};


}

#endif //SYLAR_SYLAR_TIMER_H_
