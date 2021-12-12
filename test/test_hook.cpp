//
// Created by changyuli on 11/3/21.
//

#include "hook.h"
#include "iomanager.h"
#include "log.h"

#include <random>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_random1() {
  std::default_random_engine random_engine;
  std::uniform_int_distribution<int> dist(1, 10);
  SYLAR_LOG_INFO(g_logger) << dist(random_engine);
}

void test_random2() {
  static std::default_random_engine random_engine;
  std::uniform_int_distribution<int> dist(1, 10);
  SYLAR_LOG_INFO(g_logger) << dist(random_engine);
}

void test_random() {
  SYLAR_LOG_INFO(g_logger) << "test_random1 start";
  for (int i = 0; i < 10; ++i) {
	test_random1();
  }
  SYLAR_LOG_INFO(g_logger) << "test_random1 end";
  SYLAR_LOG_INFO(g_logger) << "\n\n\n";
  SYLAR_LOG_INFO(g_logger) << "test_random2 start";
  for (int i = 0; i < 10; ++i) {
	test_random2();
  }
  SYLAR_LOG_INFO(g_logger) << "test_random2 end";
}

void test_sleep() {
  sylar::IOManager iom(1);

  iom.schedule([](){
	sleep(2);
	SYLAR_LOG_INFO(g_logger) << "sleep 2";
  });

  iom.schedule([](){
	sleep(3);
	SYLAR_LOG_INFO(g_logger) << "sleep 3";
  });

  SYLAR_LOG_INFO(g_logger) << "test_sleep";
}

int main(int agrc, char* argv[]) {
  // test_sleep();
  test_random();
  return 0;
}