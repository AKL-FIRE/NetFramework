//
// Created by changyuli on 9/26/21.
//

#ifndef SYLAR_SYLAR_UTIL_H_
#define SYLAR_SYLAR_UTIL_H_

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#include <vector>
#include <string>

namespace sylar {

pid_t GetThreadId();

uint32_t GetFiberId();

void BackTrace(std::vector<std::string>& bt, int size, int skip = 1);
std::string BacktraceToString(int size, int skip = 2, const std::string& prefix = "");

}

#endif //SYLAR_SYLAR_UTIL_H_