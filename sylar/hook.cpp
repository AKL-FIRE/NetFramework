//
// Created by changyuli on 11/3/21.
//

#include "hook.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"

#include <dlfcn.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static thread_local bool t_hook_enable = false; // hook是否可用

#define HOOK_FUN(XX) \
  XX(sleep) \
  XX(usleep) \
  XX(nanosleep) \
  XX(socket) \
  XX(connect) \
  XX(accept) \
  XX(read) \
  XX(readv) \
  XX(recv) \
  XX(recvfrom) \
  XX(recvmsg) \
  XX(write) \
  XX(writev) \
  XX(send) \
  XX(sendto) \
  XX(sendmsg) \
  XX(close) \
  XX(fcntl) \
  XX(ioctl) \
  XX(getsockopt) \
  XX(setsockopt) 

void hook_init() {
  static bool is_init = false;
  if (is_init) {
	return;
  }
  /*
   * dlsym(RTLD_NEXT, name)
   * Find the next occurrence of the desired symbol in the search order after the current object.
   * This allows one to provide a wrapper around a function in another shared object
   * */
  // 将系统调用sleep,usleep的函数指针保存在sleep_f和usleep_f中
  // 实际上劫持了系统调用，并进行组成一个函数warpper
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
  HOOK_FUN(XX);
#undef XX
}

struct _HookIniter {
  _HookIniter() {
	hook_init(); // 在main函数之前执行hook_init函数
  }
};

static _HookIniter s_hook_initer; // 在函数之前调用构造函数

bool is_hook_enable() {
 return t_hook_enable;
}

void set_hook_enable(bool flag) {
 t_hook_enable = flag;
}

struct timer_info {
  int cancelled = 0;
};

template <typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name,
    uint32_t event, int timeout_so, Args&&... args) {
  if (!sylar::t_hook_enable) {
	// 若没有开启hook,直接采用原始函数对fd进行操作
    return fun(fd, std::forward<Args>(args)...);
  }

  sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
  if (!ctx) {
    return fun(fd, std::forward<Args>(args)...);
  }

  if (ctx->isClose()) {
    // ctx已经关闭了,无效
    errno = EBADF; 
    return -1;
  }

  if (!ctx->isSocket() || ctx->getUserNonblock()) {
    // ctx不是socket文件描述符或已经被设置为非阻塞
    return fun(fd, std::forward<Args>(args)...);
  }

  uint64_t to = ctx->getTimeout(timeout_so); // 取得超时时间
  auto tinfo = std::make_shared<timer_info>();

retry:
  ssize_t n = fun(fd, std::forward<Args>(args)...);
  // 这里如果返回非负数，代表读到数据或者操作成功,直接返回函数
  while (n == -1 && errno == EINTR) {
	// 被系统中断,直接进行重试
    n = fun(fd, std::forward<Args>(args)...);
  }
  if (n == -1 && errno == EAGAIN) {
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    sylar::Timer::ptr timer;
    std::weak_ptr<timer_info> winfo(tinfo);

    if (to != (uint64_t)-1) {
      // 有设置超时，放进定时器
      timer = iom->addConditionalTimer(to, [winfo, fd, iom, event]() {
          auto t = winfo.lock();
          if (!t || t->cancelled) {
			      // 定时器不存在或设置了错误代码(不是0)
            return;
          }
		  // 定时器设置为超时，并取消该定时器
          t->cancelled = ETIMEDOUT;
          iom->cancelEvent(fd, sylar::IOManager::Event(event));
        }, winfo);
    }

	// 添加一个事件，此时cb为空，表示事件就是当前的协程，即唤醒当前协程
    int rt = iom->addEvent(fd, sylar::IOManager::Event(event));
    if (rt) {
      // 添加失败,记录日志，取消定时器，返回-1
      SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent(" << fd << ", " << event << ")";
      if (timer) {
        timer->cancel();
      }
      return -1;
    } else {
      sylar::Fiber::YieldToHold();
      // 这里被唤醒
      // 两种情况: 1. 事件被取消了 2. 有数据到来
      if (timer) {
        timer->cancel();
      }
      if (tinfo->cancelled) {
        errno = tinfo->cancelled;
        return -1;
      }

      // 到这里代表有数据抵达，回到上面重试fun函数进行io操作
      goto retry;
    }
  }

  return n;
}

}
extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
	HOOK_FUN(XX)
#undef XX

    // 若开启hook则，非阻塞sleep,让出当前线程使用权，超时后使用定时器唤醒当前协程
  unsigned int sleep(unsigned int seconds) {
	if (!sylar::t_hook_enable) {
	  return sleep_f(seconds);
	}

	sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
  iom->addTimer(seconds * 1000, std::bind((void(sylar::Scheduler::*)
          (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
        , iom, fiber, -1));
	sylar::Fiber::YieldToHold();
	return 0;
  }

// 若开启hook则，非阻塞usleep,让出当前线程使用权，超时后使用定时器唤醒当前协程
  int usleep(useconds_t usec) {
	if (!sylar::t_hook_enable) {
	  return usleep_f(usec);
	}

	sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
  iom->addTimer(usec / 1000, std::bind((void(sylar::Scheduler::*)
          (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
        , iom, fiber, -1));
	sylar::Fiber::YieldToHold();
	return 0;
  }

  int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
	  if (!sylar::t_hook_enable) {
		return nanosleep_f(rqtp, rmtp);
	  }

	  long timeout_ms = rqtp->tv_sec * 1000L + rqtp->tv_nsec / 1000000L;
	  sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
	  sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)
            (sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule
            , iom, fiber, -1));
	  sylar::Fiber::YieldToHold();
	  return 0;
  }

  int socket(int domain, int type, int protocol) {
    if (!sylar::t_hook_enable) {
      return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
      return fd;
    }
    sylar::FdMgr::GetInstance()->get(fd, true);
    return fd;
  }

  int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return connect_f(sockfd, addr, addrlen);
  }

  int accept(int s, struct sockaddr* addr, socklen_t* addrlen) {
    int fd = sylar::do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if (fd >= 0) {
      sylar::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
  }

  ssize_t read (int fildes, void *buf, size_t nbyte) {
    return sylar::do_io(fildes, read_f, "read", sylar::IOManager::READ,
        SO_RCVTIMEO, buf, nbyte);
  }

  ssize_t readv (int fildes, const struct iovec *iov, int iovcnt) {
    return sylar::do_io(fildes, readv_f, "readv", sylar::IOManager::READ,
        SO_RCVTIMEO, iov, iovcnt);
  }

  ssize_t recv (int socket, void *buffer, size_t length, int flags) {
    return sylar::do_io(socket, recv_f, "recv", sylar::IOManager::READ,
        SO_RCVTIMEO, buffer, length, flags);
  }

  ssize_t recvfrom (int socket, void * buffer, size_t length,
				 int flags, struct sockaddr * address,
				 socklen_t * address_len) {
    return sylar::do_io(socket, recvfrom_f, "recvfrom", sylar::IOManager::READ, 
        SO_RCVTIMEO, buffer, length, flags, address, address_len);
  }

  ssize_t recvmsg (int socket, struct msghdr *message, int flags) {
    return sylar::do_io(socket, recvmsg_f, "recvmsg", sylar::IOManager::READ,
        SO_RCVTIMEO, message, flags);
  }

  ssize_t write (int fildes, const void *buf, size_t nbyte) {
    return sylar::do_io(fildes, write_f, "write", sylar::IOManager::WRITE,
        SO_SNDTIMEO, buf, nbyte);
  }

  ssize_t writev (int fd, const struct iovec* iov, int iovcnt) {
    return sylar::do_io(fd, writev_f, "writev", sylar::IOManager::WRITE,
        SO_SNDTIMEO, iov, iovcnt);
  }

  ssize_t send (int s, const void* msg, size_t len, int flags) {
    return sylar::do_io(s, send_f, "send", sylar::IOManager::WRITE,
        SO_SNDTIMEO, msg, len, flags);
  }

  ssize_t sendto (int s, const void* msg, size_t len, int flags, 
      const struct sockaddr* to, socklen_t tolen) {
    return sylar::do_io(s, sendto_f, "sendto", sylar::IOManager::WRITE,
        SO_SNDTIMEO, msg, len, flags, to, tolen);
  }

  ssize_t sendmsg (int s, const struct msghdr* msg, int flags) {
    return sylar::do_io(s, sendmsg_f, "sendmsg", sylar::IOManager::WRITE,
        SO_SNDTIMEO, msg, flags);
  }

  int close(int fd) {
    if (!sylar::t_hook_enable) {
      return close_f(fd);
    }

    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if (ctx) {
      auto iom = sylar::IOManager::GetThis();
      if (iom) {
        iom->cancelAll(fd);
      }
      sylar::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
  }

  int fcntl (int fd, int cmd, ...) {

  }

  int ioctl (int d, int request, ...) {

  }

  int getsockopt (int socket, int level, int option_name,
                 void *option_value, socklen_t *option_len) {

  }

  int setsockopt (int sockfd, int level, int optname, const void* optval, socklen_t optlen) {

  }
}
