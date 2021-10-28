//
// Created by changyuli on 10/21/21.
//

#ifndef SYLAR_SYLAR_IOMANAGER_H_
#define SYLAR_SYLAR_IOMANAGER_H_

#include "scheduler.h"

namespace sylar {

class IOManager : public Scheduler {
 public:
  using ptr = std::shared_ptr<IOManager>;
  using MutexType = RWMutex;

  enum Event {
    NONE = 0x0,
    READ = 0x1, // == EPOLLIN
    WRITE = 0x4 // == EPOLLOUT
  };

 private:
  struct FdContext {
    using MutexType = Mutex;
    struct EventContext {
      Scheduler* scheduler = nullptr; // 在哪一个调度器上执行事件(事件执行的schedule)
      Fiber::ptr fiber; // 事件的协程
      std::function<void()> cb; // 事件回调
    };

    EventContext& getContext(Event event);
    void resetContext(EventContext& ctx);
    void triggerEvent(Event event);

    EventContext read; // 读事件
    EventContext write; // 写事件
	int fd = 0; // 事件关联的文件描述符
	Event events = NONE; // 已经注册的事件
	MutexType mutex;
  };

 public:
  explicit IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");
  ~IOManager();

  // 0: success; 0: retry; -1: error
  int addEvent(int fd, Event event, std::function<void()> cb = nullptr); // 添加事件event与回调函数cb到fd上
  bool delEvent(int fd, Event event); // 删除fd上注册的event事件
  bool cancelEvent(int fd, Event event); // 取消fd上注册的event事件

  bool cancelAll(int fd); // 取消fd上所有的事件

  static IOManager* GetThis(); // 获取当前线程的IOManager

 protected:
  void tickle() override;
  bool stopping() override;
  void idle() override;

  void contextResize(size_t size);

 private:
  int m_epfd = 0;
  int m_tickleFds[2];

  std::atomic<size_t> m_pendingEventCount {0};
  MutexType m_mutex;
  std::vector<FdContext*> m_fdContext;
};

}

#endif //SYLAR_SYLAR_IOMANAGER_H_
