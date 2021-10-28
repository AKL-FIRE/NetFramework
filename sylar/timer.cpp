//
// Created by changyuli on 10/28/21.
//

#include "timer.h"

#include <utility>

namespace sylar {

bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const {
  if (!lhs && !rhs) {
    return false;
  }
  if (!lhs) {
    return true;
  }
  if (!rhs) {
	return false;
  }
  if (lhs->m_next < rhs->m_next) {
	return true;
  }
  if (rhs->m_next < lhs->m_next) {
	return false;
  }
  return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager)
	: m_recurring(recurring),
	  m_ms(ms),
	  m_cb(std::move(cb)),
	  m_manager(manager) {
  m_next = sylar::GetCurrentMS() + m_ms; // 当前时间加上触发时间间隔
}

TimerManager::TimerManager() {
}

TimerManager::~TimerManager() {
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
  Timer::ptr timer(new Timer(ms, std::move(cb), recurring, this));
  RWMutexType::WriteLock lock(m_mutex);
  auto it = m_timers.insert(timer);
  bool at_front = (it.first == m_timers.begin());
  lock.unlock();

  if (at_front) {
    onTimerInsertAtFront(); // 通知有一个新加入事件排在最前面，有可能需要立刻触发
  }
  return timer;
}

Timer::ptr TimerManager::addConditionalTimer(uint64_t ms,
											 std::function<void()> cb,
											 std::weak_ptr<void> weak_cond,
											 bool recurring) {
  return sylar::Timer::ptr();
}


}