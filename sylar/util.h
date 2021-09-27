//
// Created by changyuli on 9/26/21.
//

#ifndef SYLAR_SYLAR_UTIL_H_
#define SYLAR_SYLAR_UTIL_H_

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

namespace sylar {

pid_t GetThreadId();

uint32_t GetFiberId();

}

#endif //SYLAR_SYLAR_UTIL_H_
