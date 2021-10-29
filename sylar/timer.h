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
  // 取消这个定时器
  bool cancel();
  // 更新这个定时器，当前时间 + 时间间隔
  bool refresh();
  // 重置这个定时器
  bool reset(uint64_t ms, bool from_now);

 private:
  Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager* manager);
  Timer(uint64_t next);

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
  Timer::ptr addConditionalTimer(uint64_t ms, const std::function<void()>& cb, const std::weak_ptr<void>& weak_cond, bool recurring = false);
  // 获取下一个定时器的执行时间
  uint64_t getNextTimer();
  // 返回所有已经超时的定时器任务回调
  void listExpiredCb(std::vector<std::function<void()>>& cbs);
  // 是否有定时器
  bool hasTimer();

 protected:
  virtual void onTimerInsertedAtFront() = 0;
  void addTimer(const Timer::ptr& val, RWMutexType::WriteLock& write_lock);
 private:
  bool detectClockRollover(uint64_t now_ms);
 private:
  RWMutexType m_mutex;
  std::set<Timer::ptr, Timer::Comparator> m_timers;
  bool m_tickled = false; // 是否超时了
  uint64_t m_previousTime = 0; // 上一次执行的时间
};

}

#endif //SYLAR_SYLAR_TIMER_H_
