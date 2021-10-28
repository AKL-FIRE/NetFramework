//
// Created by changyuli on 10/21/21.
//

#include "iomanager.h"
#include "log.h"
#include "macro.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace sylar {

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
	: Scheduler(threads, use_caller, name) {
  m_epfd = epoll_create(5000);
  SYLAR_ASSERT(m_epfd > 0);

  int rt = pipe(m_tickleFds);
  SYLAR_ASSERT(rt == 0);

  // 给管道读端注册可读事件
  epoll_event event {};
  // memset(&events, 0, sizeof(events));
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = m_tickleFds[0];

  rt = fcntl(event.data.fd, F_SETFL, O_NONBLOCK);
  SYLAR_ASSERT(rt == 0);

  rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
  SYLAR_ASSERT(rt == 0);

  // m_fdContext.resize(64);
  contextResize(32);

  start();
}

IOManager::~IOManager() {
  stop();
  close(m_epfd);
  close(m_tickleFds[0]);
  close(m_tickleFds[1]);

  for (auto & i : m_fdContext) {
    if (i) {
      delete i;
    }
  }
}

int IOManager::addEvent(int fd, IOManager::Event event, std::function<void()> cb) {
  FdContext* fd_context = nullptr;
  MutexType::ReadLock lock(m_mutex);
  if (m_fdContext.size() > fd) {
    fd_context = m_fdContext[fd];
  } else {
    lock.unlock();
    MutexType::WriteLock write_lock(m_mutex);
	contextResize(fd * 1.5);
	fd_context = m_fdContext[fd];
  }

  FdContext::MutexType::Lock lock1(fd_context->mutex);
  if (fd_context->events & event) {
	SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
		<< " event=" << event << " fd_context.event=" << fd_context->events;
	SYLAR_ASSERT(!(fd_context->events & event));
  }

  int op = fd_context->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
  epoll_event event1 {};
  event1.events = EPOLLET | fd_context->events | event;
  event1.data.ptr = fd_context;

  int rt = epoll_ctl(m_epfd, op, fd, &event1);
  if (rt) {
	SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
		<< op << ", " << fd << ", " << event1.events << ");"
		<< rt << " (" << errno << ") (" << strerror(errno) << ")";
	return -1;
  }

  ++m_pendingEventCount;
  fd_context->events = (Event)(fd_context->events | event);
  FdContext::EventContext& event_ctx = fd_context->getContext(event);
  SYLAR_ASSERT(!event_ctx.scheduler
  					&& !event_ctx.fiber
  					&& !event_ctx.cb);
  event_ctx.scheduler = Scheduler::GetThis();
  if (cb) {
    event_ctx.cb.swap(cb);
  } else {
    event_ctx.fiber = Fiber::GetThis();
	SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
  }
  return 0;
}

bool IOManager::delEvent(int fd, IOManager::Event event) {
  MutexType::ReadLock read_lock(m_mutex);
  if (m_fdContext.size() <= fd) {
    // fd这个描述符不存在，不需要删除
    return false;
  }
  FdContext* fd_ctx = m_fdContext[fd];
  read_lock.unlock();

  FdContext::MutexType::Lock lock(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    // 不存在event事件,不需要删除
    return false;
  }

  auto new_events = (Event) (fd_ctx->events & ~event); // 剔除event事件
  // 若new_events为空(0), 直接删除该注册事件，否则修改注册的事见
  int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent {};
  epevent.events = EPOLLET | new_events;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
	SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
							  << op << ", " << fd << ", " << epevent.events << ");"
							  << rt << " (" << errno << ") (" << strerror(errno) << ")";
	return false;
  }

  // 等待事件数减一
  --m_pendingEventCount;
  fd_ctx->events = new_events; // 更新事件
  FdContext::EventContext& event_ctx = fd_ctx->getContext(event); // 获取对应的事件上下文
  fd_ctx->resetContext(event_ctx); // 重新设置新的上下文
  return true;
}

bool IOManager::cancelEvent(int fd, IOManager::Event event) {
  MutexType::ReadLock read_lock(m_mutex);
  if (m_fdContext.size() <= fd) {
	// fd这个描述符不存在，不需要删除
	return false;
  }
  FdContext* fd_ctx = m_fdContext[fd];
  read_lock.unlock();

  FdContext::MutexType::Lock lock(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
	// 不存在event事件,不需要删除
	return false;
  }

  auto new_events = (Event) (fd_ctx->events & ~event); // 剔除event事件
  // 若new_events为空(0), 直接删除该注册事件，否则修改注册的事见
  int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent {};
  epevent.events = EPOLLET | new_events;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
	SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
							  << op << ", " << fd << ", " << epevent.events << ");"
							  << rt << " (" << errno << ") (" << strerror(errno) << ")";
	return false;
  }

  // 等待事件数减一
  --m_pendingEventCount;
  fd_ctx->triggerEvent(event);
  return true;
}

bool IOManager::cancelAll(int fd) {
  MutexType::ReadLock read_lock(m_mutex);
  if (m_fdContext.size() <= fd) {
	// fd这个描述符不存在，不需要删除
	return false;
  }
  FdContext* fd_ctx = m_fdContext[fd];
  read_lock.unlock();

  FdContext::MutexType::Lock lock(fd_ctx->mutex);
  if (!fd_ctx->events) {
	// 不存在event事件,不需要删除
	return false;
  }

  int op = EPOLL_CTL_DEL;
  epoll_event epevent {};
  epevent.events = 0;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
	SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
							  << op << ", " << fd << ", " << epevent.events << ");"
							  << rt << " (" << errno << ") (" << strerror(errno) << ")";
	return false;
  }

  if (fd_ctx->events & READ) {
	--m_pendingEventCount;
	fd_ctx->triggerEvent(READ);
  }
  if (fd_ctx->events & WRITE) {
	--m_pendingEventCount;
	fd_ctx->triggerEvent(WRITE);
  }

  SYLAR_ASSERT(fd_ctx->events == 0);
  return true;
}

IOManager *IOManager::GetThis() {
  return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::contextResize(size_t size) {
  m_fdContext.resize(size);

  for (size_t i = 0; i < m_fdContext.size(); ++i) {
    if (!m_fdContext[i]) {
      m_fdContext[i] = new FdContext;
      m_fdContext[i]->fd = i;
    }
  }
}

void IOManager::tickle() {
  if (!hasIdleThreads()) {
	return;
  }
  int rt = write(m_tickleFds[1], "T", 1);
  SYLAR_ASSERT(rt == 1);
}

bool IOManager::stopping() {
  return Scheduler::stopping() && m_pendingEventCount == 0; // 没有等待处理的事件
}

void IOManager::idle() {
  epoll_event* events = new epoll_event[64];
  std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* p) {
    delete[] p;
  });

  while (true) {
    if (stopping()) {
	  SYLAR_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping exit";
	  break;
    }

    int rt = 0;
    do {
      static const int MAX_TIMEOUT = 5000;
      rt = epoll_wait(m_epfd, events, 64, MAX_TIMEOUT);

      if (rt < 0 && errno == EINTR) {

      } else {
        break;
      }
    } while (true);

    for (int i = 0; i < rt; ++i) {
      epoll_event& event = events[i];
      if (event.data.fd == m_tickleFds[0]) {
        // event是通知tickle的事件,一次读完，防止不再触发通知
        uint8_t dummy;
        while (read(m_tickleFds[0], &dummy, 1) == 1); // 读干净为止
		continue;
      }
      // 是具体的事件
      FdContext* fd_ctx = (FdContext*)event.data.ptr;
      FdContext::MutexType::Lock lock(fd_ctx->mutex);
      if (event.events & (EPOLLERR | EPOLLHUP)) {
        event.events |= EPOLLIN | EPOLLOUT;
      }
      int real_events = NONE;
      if (event.events & EPOLLIN) {
        real_events |= READ;
      }
      if (event.events & EPOLLOUT) {
        real_events |= WRITE;
      }

      if ((fd_ctx->events & real_events) == NONE) {
        // fd_ctx没有读和写事件，可能被处理过了
		continue;
      }

      int left_events = (fd_ctx->events & ~real_events); // 剩下的事机
      int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
      event.events = EPOLLET | left_events;

      int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
      if (rt2) {
		SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
								  << op << ", " << fd_ctx->fd << ", " << event.events << ");"
								  << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
		continue;
      }

      if (real_events & READ) {
        // 触发读事件
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
      }
      if (real_events & WRITE) {
        // 触发写事件
        fd_ctx->triggerEvent(WRITE);
		--m_pendingEventCount;
	  }
    }

    // 让出执行权
    Fiber::ptr cur = Fiber::GetThis();
    auto raw_ptr = cur.get();
    cur.reset();

    raw_ptr->swapOut();
    // 这里会回到scheduler的ＭainＦunc scheduler.cpp:231
  }
}

IOManager::FdContext::EventContext &IOManager::FdContext::getContext(IOManager::Event event) {
  switch (event) {
    case READ:
      return read;
    case WRITE:
	  return write;
	default: SYLAR_ASSERT2(false, "getContext");
  }
}

void IOManager::FdContext::resetContext(IOManager::FdContext::EventContext &ctx) {
  ctx.scheduler = nullptr;
  ctx.fiber.reset();
  ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
  SYLAR_ASSERT(events & event);
  events = (Event) (events & ~event);
  EventContext& ctx = getContext(event);
  if (ctx.cb) {
    ctx.scheduler->schedule(ctx.cb);
  } else {
    ctx.scheduler->schedule(ctx.fiber);
  }
  ctx.scheduler = nullptr;
}

}