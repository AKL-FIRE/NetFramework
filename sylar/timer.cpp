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

Timer::Timer(uint64_t next)
	: m_next(next) {
}

bool Timer::cancel() {
  TimerManager::RWMutexType::WriteLock write_lock(m_manager->m_mutex);
  if (m_cb) {
    // 还未执行，因为cb有值
    m_cb = nullptr;
    auto it = m_manager->m_timers.find(shared_from_this());
    m_manager->m_timers.erase(it);
    return true;
  }
  // 已经被取消，或是已经执行完了
  return false;
}

bool Timer::refresh() {
  TimerManager::RWMutexType::WriteLock write_lock(m_manager->m_mutex);
  if (!m_cb) {
    // 回调为空，不需要刷新
	return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());
  if (it == m_manager->m_timers.end()) {
    // 没有找到该定时器，无法刷新
	return false;
  }
  // 先删除以前的版本，修改新的时间，再加回去
  // 注意！！　这里不能直接修改m_next，因为这个是set的key
  m_manager->m_timers.erase(it);
  m_next = sylar::GetCurrentMS() + m_ms;
  m_manager->m_timers.insert(shared_from_this());
  return true;
}

bool Timer::reset(uint64_t ms, bool from_now) {
  if (ms == m_ms && !from_now) {
    // 新的时间间隔等于旧的时间间隔，且不需要现在强制执行
	return true;
  }
  TimerManager::RWMutexType::WriteLock write_lock(m_manager->m_mutex);
  if (!m_cb) {
	// 回调为空，不需要刷新
	return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());
  if (it == m_manager->m_timers.end()) {
	// 没有找到该定时器，无法刷新
	return false;
  }
  // 先删除以前的版本，修改新的时间，再加回去
  // 注意！！　这里不能直接修改m_next，因为这个是set的key
  m_manager->m_timers.erase(it);
  uint64_t  start = 0;
  if (from_now) {
    // 从现在时间算起
    start = sylar::GetCurrentMS();
  } else {
    // 从原来的起点算起
    start = m_next - m_ms;
  }
  m_ms = ms;
  m_next = start + m_ms;
  // 有可能会插入到最前端
  m_manager->addTimer(shared_from_this(), write_lock);
  return true;
}

TimerManager::TimerManager() {
  m_previousTime = sylar::GetCurrentMS();
}

TimerManager::~TimerManager() {
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
  Timer::ptr timer(new Timer(ms, std::move(cb), recurring, this));
  RWMutexType::WriteLock lock(m_mutex);
  // auto it = m_timers.insert(timer);
  // bool at_front = (it.first == m_timers.begin());
  // lock.unlock();

  // if (at_front) {
  //   onTimerInsertAtFront(); // 通知有一个新加入事件排在最前面，有可能需要立刻触发
  // }
  addTimer(timer, lock);
  return timer;
}

static void OnTimer(const std::weak_ptr<void>& weak_cond, const std::function<void()>& cb) {
  std::shared_ptr<void> tmp = weak_cond.lock();
  if (tmp) {
    // 指针指向的事件依旧有效,执行回调函数
    cb();
  }
}

Timer::ptr TimerManager::addConditionalTimer(uint64_t ms,
											 const std::function<void()>& cb,
											 const std::weak_ptr<void>& weak_cond,
											 bool recurring) {
  return addTimer(ms, [weak_cond, cb] { return OnTimer(weak_cond, cb); }, recurring);
}

uint64_t TimerManager::getNextTimer() {
  RWMutexType::ReadLock lock(m_mutex);
  m_tickled = false;
  if (m_timers.empty()) {
    // 没有定时任务了
	return ~0ULL;
  }

  const Timer::ptr& next = *m_timers.begin();
  uint64_t now_ms = sylar::GetCurrentMS();
  if (now_ms >= next->m_next) {
    // 该定时器已经过时了
	return 0;
  } else {
    return next->m_next - now_ms; // 还需等待多少时间
  }
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
  uint64_t now_ms = sylar::GetCurrentMS();
  std::vector<Timer::ptr> expired;
  {
    RWMutexType::ReadLock lock(m_mutex);
    if (m_timers.empty()) {
      return;
    }
  }
  RWMutexType::WriteLock write_lock(m_mutex);
  if (m_timers.empty()) {
	return;
  }

  bool rollover = detectClockRollover(now_ms);
  if (!rollover && (*m_timers.begin())->m_next > now_ms) {
    // 没有发生回滚，且最近的时间仍大于现在的时间,不可能有超时的定时器
	return;
  }

  Timer::ptr now_timer(new Timer(now_ms)); // 当前时间的一个timer用作锚点
  // 找到第一个大于等于now_ms的定时器,且如果发生了回滚，所有定时器都被视为超时
  auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
  // 过滤掉等于的情况
  while (it != m_timers.end() && (*it)->m_next == now_ms) {
    ++it;
  }
  // 此时it要么等于end,要么是第一个大于now_ms的定时器(即还未超时)
  // m_timers开头到it上一个位置的定时器都是已经超时的
  expired.insert(expired.begin(), m_timers.begin(), it);
  m_timers.erase(m_timers.begin(), it); // 删除已经超时的定时器

  cbs.reserve(expired.size());
  for (auto& timer : expired) {
    cbs.push_back(timer->m_cb); // 事件回调返回
    if (timer->m_recurring) {
      // 是一个周期性任务,再加入定时器
      timer->m_next = now_ms + timer->m_ms;
      m_timers.insert(timer);
    } else {
      // 否则清空回调
      timer->m_cb = nullptr;
    }
  }
}

void TimerManager::addTimer(const Timer::ptr& val, RWMutexType::WriteLock& write_lock) {
  auto it = m_timers.insert(val);
  bool at_front = (it.first == m_timers.begin()) && !m_tickled;
  if (at_front) {
	m_tickled = true;
  }
  write_lock.unlock();

  if (at_front) {
	onTimerInsertedAtFront(); // 通知有一个新加入事件排在最前面，有可能需要立刻触发
  }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
  bool rollover = false;
  if (now_ms < m_previousTime && now_ms < (m_previousTime - 60 * 60 * 1000)) {
    // 如果当前时间小于之前的时间，且当前时间小于（之前时间的一小时之前），则
    // 发生了回滚，时间倒退了
    rollover = true;
  }
  m_previousTime = now_ms;
  return rollover;
}

bool TimerManager::hasTimer() {
  RWMutexType::ReadLock read_lock(m_mutex);
  return !m_timers.empty();
}

}