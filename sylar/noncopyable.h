#ifndef SYLAR_SYLAR_NONCOPYABLE_H_
#define SYLAR_SYLAR_NONCOPYABLE_H_

namespace sylar {

class NonCopyable {
  public:
    NonCopyable() = default;
    virtual ~NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

}

#endif
