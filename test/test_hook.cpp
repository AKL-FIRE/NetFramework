//
// Created by changyuli on 11/3/21.
//

#include "hook.h"
#include "iomanager.h"
#include "log.h"

#include <random>
#include <netinet/in.h>
#include <arpa/inet.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_random1() {
  std::default_random_engine random_engine;
  std::uniform_int_distribution<int> dist(1, 10);
  SYLAR_LOG_INFO(g_logger) << dist(random_engine);
}

void test_random2() {
  static std::default_random_engine random_engine;
  std::uniform_int_distribution<int> dist(1, 10);
  SYLAR_LOG_INFO(g_logger) << dist(random_engine);
}

void test_random() {
  SYLAR_LOG_INFO(g_logger) << "test_random1 start";
  for (int i = 0; i < 10; ++i) {
	test_random1();
  }
  SYLAR_LOG_INFO(g_logger) << "test_random1 end";
  SYLAR_LOG_INFO(g_logger) << "\n\n\n";
  SYLAR_LOG_INFO(g_logger) << "test_random2 start";
  for (int i = 0; i < 10; ++i) {
	test_random2();
  }
  SYLAR_LOG_INFO(g_logger) << "test_random2 end";
}

void test_sleep() {
  sylar::IOManager iom(1);

  iom.schedule([](){
	sleep(2);
	SYLAR_LOG_INFO(g_logger) << "sleep 2";
  });

  iom.schedule([](){
	sleep(3);
	SYLAR_LOG_INFO(g_logger) << "sleep 3";
  });

  SYLAR_LOG_INFO(g_logger) << "test_sleep";
}

void test_sock() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  inet_pton(AF_INET, "220.181.38.251", &addr.sin_addr.s_addr);

  SYLAR_LOG_INFO(g_logger) << "begin connect";
  int rt = connect(sock, (const struct sockaddr*)&addr, sizeof(addr));
  SYLAR_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

  if (rt) {
    return;
  }

  const char data[] = "GET / HTTP/1.0\r\n\r\n";
  rt = send(sock, data, sizeof(data), 0);
  SYLAR_LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;
  if (rt < 0) {
    return;
  }

  std::string buff;
  buff.resize(4096);

  rt = recv(sock, &buff[0], buff.size(), 0);
  SYLAR_LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;
  if (rt < 0) {
    return;
  }

  buff.shrink_to_fit();
  SYLAR_LOG_INFO(g_logger) << buff;
}

int main(int agrc, char* argv[]) {
  // test_sleep();
  // test_random();
  // test_sock();
  sylar::IOManager iom;
  iom.schedule(test_sock);
  return 0;
}
