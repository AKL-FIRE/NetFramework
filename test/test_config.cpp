//
// Created by changyuli on 9/26/21.
//

#include "../sylar/config.h"

sylar::ConfigVar<int>::ptr g_int_value_config =
	sylar::Config::Lookup("system.port", (int)8080, "system port");

sylar::ConfigVar<float>::ptr g_float_value_config =
	sylar::Config::Lookup("system.value", 7.935f, "system value");

int main(int argc, char* argv[]) {
  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << g_int_value_config->getValue();
  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << g_float_value_config->toString();
  return 0;
}