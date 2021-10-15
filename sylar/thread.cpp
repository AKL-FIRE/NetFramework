//
// Created by changyuli on 10/13/21.
//

#include "thread.h"
#include "log.h"

#include <utility>

namespace sylar {

// thread_local表示该变量每个线程都有一个独一无二的实例，且生命周期与线程相同
static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

const std::string& Thread::GetName() {
  return t_thread_name;
}

void Thread::SetName(const std::string& name) {
  if (t_thread) {
	t_thread->m_name = name;
  }
  t_thread_name = name;
}

Thread* Thread::GetThis() {
  return t_thread;
}

void Thread::join() {
  if (m_thread) {
    int rt = pthread_join(m_thread, nullptr);
    if (rt) {
	  SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt
								<< " name=" << m_name;
	  throw std::logic_error("pthread_join error");
    }
    m_thread = 0;
  }
}

Thread::Thread(std::function<void()> cb, const std::string& name)
	: m_cb(std::move(cb)),
	  m_name(name) {
  if (name.empty()) {
    m_name = "UNKNOW";
  }
  int rt = pthread_create(&m_thread, nullptr, run, this);
  if (rt) {
	SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
	<< " name=" << name;
	throw std::logic_error("pthread_create error");
  }
  m_semaphore.wait();
}

Thread::~Thread() {
  if (m_thread) {
	pthread_detach(m_thread);
  }
}

void *Thread::run(void* arg) {
  auto* thread = (Thread*)arg;
  t_thread = thread;
  t_thread_name = thread->m_name;
  thread->m_id = sylar::GetThreadId();
  pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

  std::function<void()> cb;
  cb.swap(thread->m_cb);

  thread->m_semaphore.notify(); // 线程已经运行，通知主线程构造函数完成

  cb();
  return 0;
}

Semaphore::Semaphore(uint32_t count) {
  if (sem_init(&m_semaphore, 0, count)) {
    throw std::logic_error("sem_init error");
  }
}

Semaphore::~Semaphore() {
  sem_destroy(&m_semaphore);
}

void Semaphore::wait() {
  if (sem_wait(&m_semaphore)) {
	throw std::logic_error("sem_wait error");
  }
}

void Semaphore::notify() {
  if (sem_post(&m_semaphore)) {
    throw std::logic_error("sem_post error");
  }
}

}