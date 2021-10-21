//
// Created by changyuli on 10/21/21.
//

#include "../sylar/sylar.h"

static auto g_logger = SYLAR_LOG_NAME("system");

void test_fiber() {
  static int s_count = 5;
  SYLAR_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

  sleep(1);
  if (--s_count >= 0)
  	sylar::Scheduler::GetThis()->schedule(test_fiber, sylar::GetThreadId());
}

int main(int argc, char* argv[]) {
  SYLAR_LOG_INFO(g_logger) << "main start";
  sylar::Scheduler scheduler{3, true, "test"};
  scheduler.start();
  scheduler.schedule(test_fiber);
  scheduler.stop();
  SYLAR_LOG_INFO(g_logger) << "main end";
  return 0;
}