//
// Created by changyuli on 10/16/21.
//

#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace sylar {

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr; // 协程调度器指针
static thread_local Fiber* t_fiber = nullptr; // 标识当前的协程

sylar::Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
	:m_name(name) {
  SYLAR_ASSERT(threads > 0);

  if (use_caller) {
    Fiber::GetThis(); // 初始化一个主协程
    --threads;

	SYLAR_ASSERT(GetThis() == nullptr); // 当前不存在调度器
	t_scheduler = this;

	m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
	Thread::SetName(m_name);

	t_fiber = m_rootFiber.get();
	m_rootThread = sylar::GetThreadId();
	m_threadIds.push_back(m_rootThread);
  } else {
    m_rootThread = -1;
  }
  m_threadCount = threads;
}

sylar::Scheduler::~Scheduler() {
  SYLAR_ASSERT(m_stopping);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
}

sylar::Scheduler *sylar::Scheduler::GetThis() {
  return t_scheduler;
}

sylar::Fiber *sylar::Scheduler::GetMainFiber() {
  return t_fiber;
}

void sylar::Scheduler::start() {
  MutexType::Lock lock(m_mutex);
  if (!m_stopping) {
	return;
  }
  m_stopping = false;
  SYLAR_ASSERT(m_threads.empty());

  m_threads.resize(m_threadCount);
  for (size_t i = 0; i < m_threadCount; ++i) {
    m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this),
								  m_name + "_" + std::to_string(i)));
    m_threadIds.push_back(m_threads[i]->getId());
  }

  lock.unlock();

   // if (m_rootFiber) {
   //   m_rootFiber->call();
 // 	SYLAR_LOG_INFO(g_logger) << "call out";
   // }
}

void sylar::Scheduler::stop() {
  m_autoStop = true;
  if (m_rootFiber &&
  m_threadCount == 0 &&
	  (m_rootFiber->getState() == Fiber::TERM
	  || m_rootFiber->getState() == Fiber::INIT)) {
	SYLAR_LOG_INFO(g_logger) << this << " stopped";
	m_stopping = true;

	if (stopping()) {
	  return;
	}
  }
  if (m_rootThread != -1) {
    // 表示使用use_caller的线程
	SYLAR_ASSERT(GetThis() == this);
  } else {
	SYLAR_ASSERT(GetThis() != this);
  }
  m_stopping = true;
  for (size_t i = 0; i < m_threadCount; ++i) {
    tickle();
  }

  if (m_rootFiber) {
    tickle();
  }

  if (m_rootFiber) {
//    while (!stopping()) {
//	  if (m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::EXCEPT) {
//		m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
//		SYLAR_LOG_INFO(g_logger) << " root fiber is term, reset";
//		t_fiber = m_rootFiber.get();
//	  }
//	  m_rootFiber->call();
//	}
	if (!stopping()) {
	  m_rootFiber->call();
	}
  }

  std::vector<Thread::ptr> thrs;
  {
    MutexType::Lock lock(m_mutex);
    thrs.swap(m_threads);
  }

  for (auto& i : thrs) {
    i->join();
  }
  // if (exit_on_this_fiber) {
  // }
}


void Scheduler::run() {
  SYLAR_LOG_INFO(g_logger) << "enter run function";
  set_hook_enable(true);
  Fiber::GetThis();
  setThis();
  if (GetThreadId() != m_rootThread) {
    t_fiber = Fiber::GetThis().get();
  }

  Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this))); // idle线程，没有任务时
  Fiber::ptr cb_fiber;

  FiberAndThread ft;
  while (true) {
    ft.reset();
    bool tickle_me = false;
    bool is_active = false;
	{
	  // 从消息队列中取出当前线程可以执行的任务
	  MutexType::Lock lock(m_mutex);
	  auto it = m_fibers.begin();
	  while (it != m_fibers.end()) {
	    if (it->threadId != -1 && it->threadId != GetThreadId()) {
	      // 当前协程指定了自己的线程，且不是当前线程，不进行处理
	      ++it;
	      tickle_me = true; // 发出信号自己处理不了
		  continue;
	    }

		SYLAR_ASSERT(it->fiber || it->cb); // 调度的要么是协程，要么是回调函数
	    if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
	      // it协程正在执行(可能被其他线程),不处理
	      ++it;
		  continue;
	    }

	    ft = *it; // 当前任务给ft
	    m_fibers.erase(it); // 把it任务从消息队列里面删除掉
		++m_activeThreadCount; // 激活的线程加一
		is_active = true;
		break;
	  }
	}

	if (tickle_me) {
	  tickle(); // 唤醒其他线程,有任务在等待
	}

	if (ft.fiber && ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT) {
	  // 使用协程对象
	  ft.fiber->swapIn(); // 将其唤醒
	  --m_activeThreadCount; // 激活的线程加一

	  // 这里ft->fiber协程已经退出

	  if (ft.fiber->getState() == Fiber::READY) {
	    // 可以继续执行，添加进队列
		schedule(ft.fiber);
	  } else if (ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT) {
	    // 无法继续执行，但又没有退出，进入ＨOLD状态
	    ft.fiber->setState(Fiber::HOLD);
	  }
	  ft.reset();
	} else if (ft.cb) {
	  // 使用回调函数
	  if (cb_fiber) {
	    // cb_fiber指针已经有值
	    cb_fiber->reset(ft.cb);
	  } else {
	    // cb_fiber指针以前没有值
	    cb_fiber.reset(new Fiber(ft.cb));
	  }
	  ft.reset(); // 重置ft
	  cb_fiber->swapIn();
	  --m_activeThreadCount;

	  // 执行回来
	  if (cb_fiber->getState() == Fiber::READY) {
		schedule(cb_fiber);
		cb_fiber.reset();
	  } else if (cb_fiber->getState() == Fiber::TERM || cb_fiber->getState() == Fiber::EXCEPT) {
	    cb_fiber->reset(nullptr);
	  } else {
	    cb_fiber->setState(Fiber::HOLD);
	    cb_fiber.reset();
	  }
	} else {
	  if (is_active) {
	    --m_activeThreadCount;
		continue;
	  }
	  // 没有携程，没有回调，无事可做,执行idle部分
	  if (idle_fiber->getState() == Fiber::TERM) {
	    // idle线程也无事可做，直接退出
		SYLAR_LOG_INFO(g_logger) << "idle fiber term";
	    break;
	  }

	  ++m_idleThreadCount;
	  idle_fiber->swapIn();
	  --m_idleThreadCount;
	  if (idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT) {
	    idle_fiber->setState(Fiber::HOLD);
	  }
	}
  }
}

void Scheduler::setThis() {
  t_scheduler = this;
}

bool Scheduler::stopping() {
  MutexType::Lock lock(m_mutex);
  return m_autoStop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle() {
  SYLAR_LOG_INFO(g_logger) << "idle";
  while (!stopping()) {
    Fiber::YieldToHold();
  }
}

void sylar::Scheduler::tickle() {
  SYLAR_LOG_INFO(g_logger) << "tickle";
}

}
