//
// Created by changyuli on 10/15/21.
//

#ifndef SYLAR_SYLAR_FIBER_H_
#define SYLAR_SYLAR_FIBER_H_

#include <ucontext.h>

#include <memory>
#include <functional>

#include "thread.h"

namespace sylar {

class Fiber : public std::enable_shared_from_this<Fiber> {
 private:
  Fiber();
 public:
  using ptr = std::shared_ptr<Fiber>;

  enum State {
    INIT, // 初始化状态
    HOLD, // 挂起
    EXEC, // 正在执行
    TERM, // 终止
    READY, // 准备好执行
    EXCEPT // 异常状态
  };

  Fiber(std::function<void()> cb, size_t stackSize = 0);
  ~Fiber();

  void reset(std::function<void()> cb); // 重置协程函数,并重置状态(INIT, TERM)
  void swapIn(); // 切换到当前协程（请求执行权）
  void swapOut(); // 切换到后台（把执行权让出来给主协程）
  uint64_t getId() const {return m_id;}

  static void SetThis(Fiber* f); // 设置当前协程
  static Fiber::ptr GetThis(); // 获取当前的子协程，如果不存在子协程，则创建一个主协程
  static void YieldToReady(); // 当前协程切换到后台,并且设置为Ready状态
  static void YieldToHold(); //  当前协程切换到后台,并且设置为Hold状态
  static uint64_t TotalFibers(); // 返回总的协程数
  static void MainFunc(); // 协程执行的函数
  static uint64_t GetFiberId();

 private:
  uint64_t m_id = 0;
  uint32_t m_stacksize = 0;
  State m_state = INIT;

  ucontext_t m_ctx;
  void* m_stack = nullptr;

  std::function<void()> m_cb;
 };

}

#endif //SYLAR_SYLAR_FIBER_H_
