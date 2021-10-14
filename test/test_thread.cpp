//
// Created by changyuli on 10/14/21.
//

#include "../sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int count = 0;
sylar::RWMutex s_mutex;
sylar::Mutex s_mutex1;

void fun1() {
  SYLAR_LOG_INFO(g_logger) << "name: " << sylar::Thread::GetName()
  							<< " this.name: " << sylar::Thread::GetThis()->getName()
  							<< " id: " << sylar::GetThreadId()
  							<< " this.id: " << sylar::Thread::GetThis()->getId();
  for(int i = 0; i < 100000; ++i) {
    // sylar::RWMutex::WriteLock lock(s_mutex);
    sylar::Mutex::Lock lock(s_mutex1);
    ++count;
  }
}

void fun2() {

}

int main(int argc, char* argv[]) {
  SYLAR_LOG_INFO(g_logger) << "thread test begin";
  std::vector<sylar::Thread::ptr> threads;
  for (int i = 0; i < 5; ++i) {
    auto thr = std::make_shared<sylar::Thread>(fun1, "name_" + std::to_string(i));
    threads.push_back(thr);
  }

  for (int i = 0; i < 5; ++i) {
    threads[i]->join();
  }
  SYLAR_LOG_INFO(g_logger) << "thread test end";
  SYLAR_LOG_INFO(g_logger) << "count=" << count;
}