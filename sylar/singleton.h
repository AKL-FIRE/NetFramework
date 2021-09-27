//
// Created by changyuli on 9/26/21.
//

#ifndef SYLAR_SYLAR_SINGLETON_H_
#define SYLAR_SYLAR_SINGLETON_H_

namespace sylar {

template <typename T, typename X = void, int N = 0>
class Singleton {
 public:
  Singleton() = delete;
  Singleton(const Singleton&) = delete;
  static T* GetInstance() {
	static T val;
	return &val;
  }
};

template <typename T, typename X = void, int N = 0>
class SingletonPtr {
 public:
  SingletonPtr() = delete;
  SingletonPtr(const SingletonPtr&) = delete;
  static std::shared_ptr<T> GetInstance() {
	static std::shared_ptr<T> val(new T);
	return val;
  }
};

}



#endif //SYLAR_SYLAR_SINGLETON_H_
