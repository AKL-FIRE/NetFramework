//
// Created by changyuli on 9/26/21.
//

#include "util.h"
#include "log.h"
#include "fiber.h"
#include <execinfo.h>

namespace sylar {

Logger::ptr g_logger = SYLAR_LOG_NAME("system");

pid_t GetThreadId() {
  return syscall(SYS_gettid);
}

uint32_t GetFiberId() {
  return Fiber::GetFiberId();
}

void BackTrace(std::vector<std::string>& bt, int size, int skip) {
  void** array = (void**)malloc(sizeof(void*) * size);
  size_t s = backtrace(array, size);

  char** strings = backtrace_symbols(array, s);
  if (strings == nullptr) {
	SYLAR_LOG_ERROR(g_logger) << "backtrace_symbols error";
	free(strings);
	return;
  }
  for (size_t i = skip; i < s; ++i) {
    bt.emplace_back(strings[i]);
  }
  free(strings);
  free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
  std::vector<std::string> bt;
  BackTrace(bt, size, skip);
  std::stringstream ss;
  for (auto& i : bt) {
    ss << prefix << i << std::endl;
  }
  return ss.str();
}
}
