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

[[noreturn]]
void fun2() {
  while (true) {
	SYLAR_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  }
}

[[noreturn]]
void fun3() {
  while(true) {
	SYLAR_LOG_INFO(g_logger) << "===========================================";
  }
}

int main(int argc, char* argv[]) {
  SYLAR_LOG_INFO(g_logger) << "thread test begin";
  YAML::Node root = YAML::LoadFile("/home/changyuli/Documents/Project/Repos/sylar/test/resource/log2.yml");
  sylar::Config::LoadFromYAML(root);

  std::vector<sylar::Thread::ptr> threads;
  for (int i = 0; i < 2; ++i) {
    auto thr1 = std::make_shared<sylar::Thread>(fun2, "name_" + std::to_string(i * 2));
	auto thr2 = std::make_shared<sylar::Thread>(fun3, "name_" + std::to_string(i * 2 + 1));
	threads.push_back(thr1);
	threads.push_back(thr2);
  }

  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
  SYLAR_LOG_INFO(g_logger) << "thread test end";
  SYLAR_LOG_INFO(g_logger) << "count=" << count;

}
