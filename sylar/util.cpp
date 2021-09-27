//
// Created by changyuli on 9/26/21.
//

#include "util.h"


namespace sylar {

pid_t GetThreadId() {
  return syscall(SYS_gettid);
}

uint32_t GetFiberId() {
  return 0;
}

}
