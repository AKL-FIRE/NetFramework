//
// Created by changyuli on 10/15/21.
//

#include "../sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_assert() {
  SYLAR_LOG_INFO(g_logger) << sylar::BacktraceToString(10);
  SYLAR_ASSERT2(0 == 1, "abcd");
}

int main(int argc, char* argv[]) {
  test_assert();
  return 0;
}