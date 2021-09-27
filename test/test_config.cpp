//
// Created by changyuli on 9/26/21.
//

#include "../sylar/config.h"

#include <yaml-cpp/yaml.h>

sylar::ConfigVar<int>::ptr g_int_value_config =
	sylar::Config::Lookup("system.port", (int)8080, "system port");

sylar::ConfigVar<float>::ptr g_float_value_config =
	sylar::Config::Lookup("system.value", 7.935f, "system value");

sylar::ConfigVar<std::vector<int>>::ptr g_int_vector_value_config =
	sylar::Config::Lookup("system.int_vec", std::vector<int> {1, 2}, "system int vector");

void print_yaml(const YAML::Node& node, int level) {
  if (node.IsScalar()) {
	SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') << node.Scalar() << " - " << node.Type();
  } else if (node.IsNull()) {
	SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) <<  std::string(level * 4, ' ') << "NULL - " << node.Type() << " - " << level;
  } else if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
	  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') << it->first << " - " << it->second.Type() << " - " << level;
	  print_yaml(it->second, level + 1);
    }
  } else if (node.IsSequence()) {
    for (size_t i = 0; i < node.size(); ++i) {
	  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ') << i << " - " << node[i].Type() << " - " << level;
	  print_yaml(node[i], level + 1);
    }
  }
}

void test_yaml() {
  YAML::Node root = YAML::LoadFile("/home/changyuli/Documents/Project/Repos/sylar/test/resource/log.yml");
  print_yaml(root, 0);
  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << root;
}

void test_config() {
  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " << g_int_value_config->getValue();
  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " << g_float_value_config->toString();

  auto v = g_int_vector_value_config->getValue();
  for(auto& e : v) {
	SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before int_vec " << e;
  }

  YAML::Node root = YAML::LoadFile("/home/changyuli/Documents/Project/Repos/sylar/test/resource/log.yml");
  sylar::Config::LoadFromYAML(root);

  v = g_int_vector_value_config->getValue();
  for(auto& e : v) {
	SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after int_vec " << e;
  }

  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " << g_int_value_config->getValue();
  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " << g_float_value_config->toString();
}

int main(int argc, char* argv[]) {
  // test_yaml();
  test_config();
  return 0;
}