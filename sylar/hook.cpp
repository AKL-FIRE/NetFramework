//
// Created by changyuli on 11/3/21.
//

#include "hook.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "log.h"
#include "config.h"

#include <dlfcn.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout = 
  sylar::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

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

static uint64_t s_connect_timeout = -1;
struct _HookIniter {
  _HookIniter() {
	  hook_init(); // 在main函数之前执行hook_init函数
    s_connect_timeout = g_tcp_connect_timeout->getValue();

    g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value) {
        SYLAR_LOG_INFO(g_logger) << "tcp connect timeout changed from " << old_value << " to " << new_value;
        s_connect_timeout = new_value;
      });
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

#include <stdarg.h>

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

  // 用于定义带有超时功能的connect函数
  int connect_with_timeout(int sockfd, const struct sockaddr* addr,
      socklen_t addrlen, uint64_t timeout_ms) {
    if (!sylar::t_hook_enable) {
      return connect_f(sockfd, addr, addrlen);
    }
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(sockfd);
    if (!ctx || ctx->isClose()) {
      errno = EBADF;
      return -1;
    }

    if (!ctx->isSocket()) {
      return connect_f(sockfd, addr, addrlen);
    }

    int n = connect_f(sockfd, addr, addrlen);
    if (n == 0) {
      return 0;
    } else if (n != -1 || errno != EINPROGRESS) {
      return n;
    }

    sylar::IOManager* iom = sylar::IOManager::GetThis();
    sylar::Timer::ptr timer;
    auto tinfo = std::make_shared<sylar::timer_info>();
    std::weak_ptr<sylar::timer_info> winfo(tinfo);

    if (timeout_ms != (uint64_t)-1) {
      // 设置定时器
      timer = iom->addConditionalTimer(timeout_ms, [winfo, sockfd, iom]() {
          auto t = winfo.lock();
          if (!t || t->cancelled) {
            return;
          }
          t->cancelled = ETIMEDOUT;
          iom->cancelEvent(sockfd, sylar::IOManager::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(sockfd, sylar::IOManager::WRITE);
    if (rt == 0) {
      sylar::Fiber::YieldToHold();
      if (timer) {
        timer->cancel();
      }
      if (tinfo->cancelled) {
        errno = tinfo->cancelled;
        return -1;
      }
    } else {
      if (timer) {
        timer->cancel();
      }
      SYLAR_LOG_ERROR(sylar::g_logger) << "connect addEvent(" << sockfd << ", WRITE) error";
    }
    int error = 0;
    socklen_t len = sizeof(int);
    if (-1 == getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len)) {
      return -1;
    }
    if (error == 0) {
      return 0;
    } else {
      errno = error;
      return -1;
    }
  }

  int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);
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
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
      case F_SETFL:
        {
          int arg = va_arg(va, int);
          va_end(va);
          sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
          if (!ctx || ctx->isClose() || !ctx->isSocket()) {
            return fcntl_f(fd, cmd, arg);
          }
          ctx->setUserNonblock(arg & O_NONBLOCK);
          if (ctx->getSysNonblock()) {
            arg |= O_NONBLOCK;
          } else {
            arg &= ~O_NONBLOCK;
          }
          return fcntl_f(fd, cmd, arg);
        }
        break;
      case F_GETFL:
        {
          va_end(va);
          int arg = fcntl_f(fd, cmd);
          sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
          if (!ctx || ctx->isClose() || !ctx->isSocket()) {
            return arg;
          }
          if (ctx->getUserNonblock()) {
            return arg | O_NONBLOCK;
          } else {
            return arg & ~O_NONBLOCK;
          }
        }
        break;
      case F_DUPFD:
      case F_DUPFD_CLOEXEC:
      case F_SETFD:
      case F_SETOWN:
      case F_SETSIG:
      case F_SETLEASE:
      case F_NOTIFY:
      case F_SETPIPE_SZ:
        {
          int arg = va_arg(va, int);
          va_end(va);
          return fcntl_f(fd, cmd, arg);
        }
        break;
      case F_GETFD:
      case F_GETOWN:
      case F_GETSIG:
      case F_GETLEASE:
      case F_GETPIPE_SZ:
        {
          return fcntl_f(fd, cmd);
        }
        break;
      case F_SETLK:
      case F_SETLKW:
      case F_GETLK:
        {
          struct flock* arg = va_arg(va, struct flock*);
          va_end(va);
          return fcntl_f(fd, cmd, arg);
        }
        break;
      case F_GETOWN_EX:
      case F_SETOWN_EX:
        {
          struct f_owner_ex* arg = va_arg(va, struct f_owner_ex*);
          va_end(va);
          return fcntl_f(fd, cmd, arg);
        }
        break;
      default:
        va_end(va);
        return fcntl_f(fd, cmd);
        break;
    }
  }

  int ioctl (int d, unsigned long request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if (FIONBIO == request) {
      bool user_nonblock = !!*(int*)arg;
      sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(d);
      if (!ctx || ctx->isClose() || !ctx->isSocket()) {
        return ioctl_f(d, request, arg);
      }
      ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
  }

  int getsockopt (int socket, int level, int option_name,
                 void *option_value, socklen_t *option_len) {
    return getsockopt_f(socket, level, option_name, option_value, option_len); 
  }

  int setsockopt (int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    if (!sylar::t_hook_enable) {
      return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if (level == SOL_SOCKET) {
      if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(sockfd);
        if (ctx) {
          const timeval* tv = (const timeval*)optval;
          ctx->setTimeout(optname, tv->tv_sec * 1000 + tv->tv_usec / 1000);
        }
      }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
  }
}
