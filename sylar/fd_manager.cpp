#include "fd_manager.h"
#include "hook.h"

#include <sys/stat.h>

namespace sylar {


FdCtx::FdCtx(int fd)
    :m_isInit(false)
    ,m_isSocket(false) 
    ,m_userNonblock(false)
    ,m_sysNonblock(false)
    ,m_isClosed(false) 
    ,m_fd(fd)
    ,m_recvTimeout(-1)
    ,m_sendTimeout(-1) {
      init();
    }

FdCtx::~FdCtx() {}

bool FdCtx::init() {
  if (m_isInit) {
    return true;
  }
  m_recvTimeout = -1;
  m_sendTimeout = -1;

  struct stat fd_stat;
  if (fstat(m_fd, &fd_stat) == -1) {
    // error
    m_isInit = false;
    m_isSocket = false;
  } else {
    m_isInit = true;
    m_isSocket = S_ISSOCK(fd_stat.st_mode);
  }

  if (m_isSocket) {
    // 当前是一个socket
    int flags = fcntl_f(m_fd, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
      // 将这个socket设置为非阻塞
      fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
    }
    m_sysNonblock = true;
  } else {
    m_sysNonblock = false;
  }

  m_userNonblock = false;
  m_isClosed = false;
  return m_isInit;
}

void FdCtx::setTimeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    m_recvTimeout = v;
  } else {
    m_sendTimeout = v;
  }
}

uint64_t FdCtx::getTimeout(int type) {
  if (type == SO_RCVTIMEO) {
    return m_recvTimeout;
  } else {
    return m_sendTimeout;
  }
}

FdManager::FdManager() {
  m_datas.resize(0);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
  RWMutexType::ReadLock lock(m_mutex);
  if (m_datas.size() <= fd) {
    // 这里fd越界
    if (!auto_create)
      return nullptr;
  } else {
    if (m_datas[fd] || !auto_create) {
      return m_datas[fd];
    }
  }
  lock.unlock();

  RWMutexType::WriteLock write_lock(m_mutex);
  auto ctx = std::make_shared<FdCtx>(fd);
  m_datas[fd] = ctx;
  return ctx;
}

void FdManager::del(int fd) {
  RWMutexType::WriteLock lock(m_mutex);
  if (m_datas.size() <= fd) {
    return;
  } 
  m_datas[fd].reset();
}

}
