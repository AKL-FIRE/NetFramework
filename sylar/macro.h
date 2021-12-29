//
// Created by changyuli on 10/15/21.
//

#ifndef SYLAR_SYLAR_MACRO_H_
#define SYLAR_SYLAR_MACRO_H_

#include "util.h"

#include <string>
#include <cassert>

#if defined (__GNUC__) || defined (__llvm__)
  #define SYLAR_LICKLY(x) __builtin_expect(!!(x), 1)
  #define SYLAR_UNLICKLY(x) __builtin_expect(!!(x), 0)
#else
  #define SYLAR_LICKLY(x) (x)
  #define SYLAR_UNLICKLY(x) (x)
#endif

#define SYLAR_ASSERT(x) \
  if (SYLAR_UNLICKLY(!(x))) {           \
    SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << " ASSERTION: " #x \
  		<< "\nbacktrace:\n" << sylar::BacktraceToString(100, 2, "    "); \
  	assert(x); \
  }

#define SYLAR_ASSERT2(x, w) \
  if (SYLAR_UNLICKLY(!(x))) {           \
	  SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << " ASSERTION: " #x \
          << "\n" << (w)   \
		  << "\nbacktrace:\n" << sylar::BacktraceToString(100, 2, "    "); \
	  assert(x); \
	}

#endif //SYLAR_SYLAR_MACRO_H_
