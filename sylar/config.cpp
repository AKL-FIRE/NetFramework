//
// Created by changyuli on 9/26/21.
//

#include "config.h"

namespace sylar {


// "A.B", 10
/*
 * A:
 * 	B: 10
 * 	C: str
 * */

static void ListAllMember(const std::string& prefix,
						  const YAML::Node& node,
						  std::list<std::pair<std::string, const YAML::Node>>& output) {
  if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
	SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Config invalid name: " << prefix << " : " << node;
	return;
  }
  output.emplace_back(prefix, node);
  if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
	  ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
    }
  }
}

void Config::LoadFromYAML(const YAML::Node &root) {
  std::list<std::pair<std::string, const YAML::Node>> all_nodes;
  ListAllMember("", root, all_nodes);

  for (auto& e : all_nodes) {
    std::string key = e.first;
    if (key.empty()) {
	  continue;
    }

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    auto var = LookupBase(key);
    if (var) {
      if (e.second.IsScalar()) {
        var->fromString(e.second.Scalar());
      } else {
        std::stringstream ss;
        ss << e.second;
        var->fromString(ss.str());
      }
    }
  }
}

ConfigVarBase::ptr Config::LookupBase(const std::string &name) {
  RWMutexType::ReadLock lock(GetMutex());
  auto it = Config::GetDatas().find(name);
  return it == Config::GetDatas().end() ? nullptr : it->second;
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
  RWMutexType::ReadLock lock(GetMutex());
  ConfigVarMap& m = GetDatas();
  for (auto & it : m) {
    cb(it.second);
  }
}

}