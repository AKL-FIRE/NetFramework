//
// Created by changyuli on 10/16/21.
//


#include "../sylar/sylar.h"

auto g_logger = SYLAR_LOG_ROOT();

void run_in_fiber() {
  SYLAR_LOG_INFO(g_logger) << "run_in_fiber begin";
  sylar::Fiber::YieldToHold();
  SYLAR_LOG_INFO(g_logger) << "run_in_fiber end";
  sylar::Fiber::YieldToHold();
}

void test_fiber() {
  SYLAR_LOG_INFO(g_logger) << "main begin -1";
  {
	sylar::Fiber::GetThis();
	SYLAR_LOG_INFO(g_logger) << "main begin";
	sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));
	fiber->swapIn();
	SYLAR_LOG_INFO(g_logger) << "main after swapIn";
	fiber->swapIn();
	SYLAR_LOG_INFO(g_logger) << "main after end";
	fiber->swapIn();
  }
  SYLAR_LOG_INFO(g_logger) << "main after end2";
}

int main(int argc, char* argv[]) {
  sylar::Thread::SetName("main");

  std::vector<sylar::Thread::ptr> thrs;
  thrs.reserve(3);
  for (int i = 0; i < 3; ++i) {
    thrs.push_back(std::make_shared<sylar::Thread>(test_fiber, "name_" + std::to_string(i)));
  }

  for (auto& e : thrs) {
    e->join();
  }

  return 0;
}