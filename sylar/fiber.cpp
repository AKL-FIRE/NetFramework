//
// Created by changyuli on 10/15/21.
//

#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"

#include <atomic>
#include <utility>

namespace sylar {

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber* t_fiber = nullptr; // 当前正在执行的协程
static thread_local Fiber::ptr t_threadFiber = nullptr; // 主协程

static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
	Config::Lookup<uint32_t>("fiber.stack_size",
							 1024 * 1024,
							 "fiber stack size");

class MallocStackAllocator {
 public:
  static void* Alloc(size_t size) {
    return malloc(size);
  }

  static void Dealloc(void* vp, size_t size) {
	free(vp);
  }
};

using StackAllocator = MallocStackAllocator;

Fiber::Fiber(std::function<void()> cb, size_t stackSize, bool use_caller)
	:m_id(++s_fiber_id),
	m_cb(std::move(cb)) {
  ++s_fiber_count;
  m_stacksize = stackSize ? stackSize : g_fiber_stack_size->getValue();

  m_stack = StackAllocator::Alloc(m_stacksize); // 创建栈内存
  // 获取当前上下文
  if (getcontext(&m_ctx)) {
	SYLAR_ASSERT2(false, "getcontext");
  }
  m_ctx.uc_link = nullptr; // 后继执行上下文
  m_ctx.uc_stack.ss_sp = m_stack; // 设置新创建的栈地址
  m_ctx.uc_stack.ss_size = m_stacksize; // 设置新栈的大小

  if (!use_caller) {
	makecontext(&m_ctx, MainFunc, 0);
  } else {
	makecontext(&m_ctx, CallerMainFunc, 0);
  }
  SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
}

Fiber::~Fiber() {
  --s_fiber_count;
  if (m_stack) {
    // 子协程
	SYLAR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
	StackAllocator::Dealloc(m_stack, m_stacksize);
  } else {
    // 主协程
	SYLAR_ASSERT(!m_cb);
	SYLAR_ASSERT(m_state == EXEC);

	Fiber* cur = t_fiber;
	if (cur == this) {
	  SetThis(nullptr);
	}
  }
  SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << m_id;
}

void Fiber::reset(std::function<void()> cb) {
  SYLAR_ASSERT(m_stack); // 必须是子协程
  SYLAR_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT); // 该协程当前状态必须是终止或者初始化

  m_cb = std::move(cb); // 更改回调函数
  if (getcontext(&m_ctx)) {
	SYLAR_ASSERT2(false, "getcontext");
  }

  // 重置上下文信息
  m_ctx.uc_link = nullptr;
  m_ctx.uc_stack.ss_sp = m_stack;
  m_ctx.uc_stack.ss_size = m_stacksize;

  // 根据配置创建一个上下文
  makecontext(&m_ctx, MainFunc, 0);
  // 新的上下文状态是初始化
  m_state = INIT;
}

void Fiber::swapIn() {
  SetThis(this); // 设置当前协程为this
  SYLAR_ASSERT(m_state != EXEC);

  // 当前上下文将要执行
  m_state = EXEC;

  // 把主协程的上下文保存在t_threadFiber->m_ctx中，并激活当前上下文m_ctx
  if (swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
	SYLAR_ASSERT2(false, "swapcontext");
  }
}

void Fiber::swapOut() {
  // SetThis(t_threadFiber.get());  // 设置当前协程为主协程
  SetThis(Scheduler::GetMainFiber());
  if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {
	SYLAR_ASSERT2(false, "swapcontext");
  }
}

void Fiber::call() {
  SetThis(this);
  m_state = EXEC;
  if (swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
	SYLAR_ASSERT2(false, "swapcontext");
  }
}

void Fiber::back() {
  SetThis(t_threadFiber.get());
  if (swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
	SYLAR_ASSERT2(false, "swapcontext");
  }
}

Fiber::ptr Fiber::GetThis() {
  if (t_fiber) {
    return t_fiber->shared_from_this();
  }
  Fiber::ptr main_fiber(new Fiber);
  SYLAR_ASSERT(t_fiber == main_fiber.get());
  t_threadFiber = main_fiber;
  return t_fiber->shared_from_this();
}

void Fiber::YieldToReady() {
  Fiber::ptr cur = GetThis();
  cur->m_state = READY;
  cur->swapOut();
}

void Fiber::YieldToHold() {
  Fiber::ptr cur = GetThis();
  cur->m_state = HOLD;
  cur->swapOut();
}

uint64_t Fiber::TotalFibers() {
  return s_fiber_count;
}

void Fiber::MainFunc() {
  Fiber::ptr cur = GetThis(); // 获得当前协程
  SYLAR_ASSERT(cur);
  try {
    cur->m_cb(); // 执行该协程的回调函数
    cur->m_cb = nullptr;
    cur->m_state = TERM;
  } catch (std::exception& ex) {
    cur->m_state = EXCEPT;
	SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
	<< " fiber_id=" << cur->getId()
	<< std::endl << BacktraceToString(10);
  } catch (...) {
	cur->m_state = EXCEPT;
	SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
		  << " fiber_id=" << cur->getId()
	<< std::endl << BacktraceToString(10);
  }

  // 注意！！！
  /*
   * 这里的cur智能指针的引用计数无法自动减一，
   * 这是由于swapOut之后,这里的栈对象依旧存在，
   * 因此我们需要手动给智能指针减一引用
   * */
  auto raw_ptr = cur.get();
  cur.reset(); // 这里调用的是智能指针类的成员函数
  raw_ptr->swapOut();

  SYLAR_ASSERT2(false, "never reach here fiber_id=" + std::to_string(raw_ptr->getId()));
}

void Fiber::CallerMainFunc() {
  Fiber::ptr cur = GetThis(); // 获得当前协程
  SYLAR_ASSERT(cur);
  try {
	cur->m_cb(); // 执行改协程的回调函数
	cur->m_cb = nullptr;
	cur->m_state = TERM;
  } catch (std::exception& ex) {
	cur->m_state = EXCEPT;
	SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
							  << " fiber_id=" << cur->getId()
							  << std::endl << BacktraceToString(10);
  } catch (...) {
	cur->m_state = EXCEPT;
	SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
							  << " fiber_id=" << cur->getId()
							  << std::endl << BacktraceToString(10);
  }

  // 注意！！！
  /*
   * 这里的cur智能指针的引用计数无法自动减一，
   * 这是由于swapOut之后,这里的栈对象依旧存在，
   * 因此我们需要手动给智能指针减一引用
   * */
  auto raw_ptr = cur.get();
  cur.reset(); // 这里调用的是智能指针类的成员函数
  raw_ptr->back();

  SYLAR_ASSERT2(false, "never reach here fiber_id=" + std::to_string(raw_ptr->getId()));
}

Fiber::Fiber() {
  //　初始化主协程
  m_state = EXEC;
  SetThis(this);

  if (getcontext(&m_ctx)) {
	SYLAR_ASSERT2(false, "getcontext");
  }

  ++s_fiber_count;

  SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

void Fiber::SetThis(Fiber *f) {
  t_fiber = f;
}

uint64_t Fiber::GetFiberId() {
  if (t_fiber) {
    return t_fiber->getId();
  }
  return 0;
}



}