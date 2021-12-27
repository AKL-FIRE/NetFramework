#include "address.h"
#include "log.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test() {
  std::vector<sylar::Address::ptr> address;
  auto v = sylar::Address::Lookup(address, "www.baidu.com:http");
  if (!v) {
    SYLAR_LOG_ERROR(g_logger) << "lookup fail";
    return;
  }

  for (size_t i = 0; i < address.size(); ++i) {
    SYLAR_LOG_INFO(g_logger) << i << " - " << address[i]->toString();
  }
}

void test_iface() {
  std::multimap<std::string, std::pair<sylar::Address::ptr, uint32_t>> results;

  auto v = sylar::Address::GetInterfaceAddress(results);
  if (!v) {
    SYLAR_LOG_ERROR(g_logger) << "GetInterfaceAddress fail";
    return;
  }
  for (auto& i : results) {
    SYLAR_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - " << i.second.second;
  }
}

void test_ipv4() {
  auto addr = sylar::IPAddress::Create("127.0.0.1", 10086);
  if (addr) {
    SYLAR_LOG_INFO(g_logger) << addr->toString();
  }
}

int main(int argc, char* argv[]) {
  // test();
  // test_iface();
  test_ipv4();
  return 0;
}
