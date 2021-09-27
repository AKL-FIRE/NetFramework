//
// Created by changyuli on 9/26/21.
//

#ifndef SYLAR_SYLAR_CONFIG_H_
#define SYLAR_SYLAR_CONFIG_H_

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include <boost/lexical_cast.hpp>

#include "log.h"

namespace sylar {

class ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVarBase>;
  explicit ConfigVarBase(std::string  name, std::string  description = "")
  	: m_name(std::move(name)), m_description(std::move(description)) {}
  virtual ~ConfigVarBase() = default;

  const std::string& getName() const {return m_name;}
  const std::string& getDescription() const {return m_description;}

  virtual std::string toString() = 0;
  virtual bool fromString(const std::string& val) = 0;
 protected:
  std::string m_name;
  std::string m_description;
};

template<typename T>
class ConfigVar : public ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVar>;

  ConfigVar(const std::string& name,
			const T& default_value,
			const std::string& description = "")
			: ConfigVarBase(name, description),
			m_val(default_value) {}

  std::string toString() override {
	try {
	  return boost::lexical_cast<std::string>(m_val);
	} catch (std::exception& e) {
	  SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception" <<
	  e.what() << " convert: " << typeid(m_val).name() << " to string ";
	}
	return "";
  }

  bool fromString(const std::string& val) override {
	try {
	  m_val = boost::lexical_cast<T>(val);
	  return true;
	} catch (std::exception& e) {
	  SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception" <<
										e.what() << " convert: string to " << typeid(m_val).name();
	}
	return false;
  }

  const T& getValue() {return m_val;}
  void setValue(const T& v) {m_val = v;}

 private:
  T m_val;
};

class Config {
 public:
  using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;

  template<typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name,
										   const T& default_value,
										   const std::string& description = "") {
    auto tmp = Lookup<T>(name);
    // 已经有了就立刻返回存在的
    if (tmp) {
	  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
	  return tmp;
    }

    // 判断名字是否合法
    if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._0123456789") != std::string::npos) {
	  SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
      throw std::invalid_argument(name);
    }

    // 创建并插入
    auto v = std::make_shared<ConfigVar<T>>(name, default_value, description);
    s_data[name] = v;
    return v;
  }

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
    auto it = s_data.find(name);
    if (it == s_data.end()) {
	  return nullptr;
    }
    return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
  }
 private:
  static ConfigVarMap s_data;
};



}

#endif //SYLAR_SYLAR_CONFIG_H_
