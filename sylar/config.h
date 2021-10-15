//
// Created by changyuli on 9/26/21.
//

#ifndef SYLAR_SYLAR_CONFIG_H_
#define SYLAR_SYLAR_CONFIG_H_

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <functional>

#include <boost/lexical_cast.hpp>
#include <yaml-cpp/yaml.h>

#include "log.h"
#include "thread.h"

namespace sylar {

class ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVarBase>;
  explicit ConfigVarBase(std::string  name, std::string  description = "")
  	: m_name(std::move(name)), m_description(std::move(description)) {
    std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
  }
  virtual ~ConfigVarBase() = default;

  const std::string& getName() const {return m_name;}
  const std::string& getDescription() const {return m_description;}
  virtual std::string getTypeName() const = 0;

  virtual std::string toString() = 0;
  virtual bool fromString(const std::string& val) = 0;
 protected:
  std::string m_name;
  std::string m_description;
};

// From from_type, To to_type
template <typename From, typename To>
class LexicalCast {
 public:
  To operator()(const From& f) {
    return boost::lexical_cast<To>(f);
  }
};

template <typename T>
class LexicalCast<std::string, std::vector<T>> {
 public:
  std::vector<T> operator()(const std::string& str) {
    auto node = YAML::Load(str);
    std::vector<T> vec;
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.push_back(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

template <typename T>
class LexicalCast<std::vector<T>, std::string> {
 public:
  std::string operator()(const std::vector<T>& v) {
    YAML::Node node;
    for (auto& e : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(e)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};


template <typename T>
class LexicalCast<std::string, std::list<T>> {
 public:
  std::list<T> operator()(const std::string& str) {
	auto node = YAML::Load(str);
	std::list<T> vec;
	std::stringstream ss;
	for (size_t i = 0; i < node.size(); ++i) {
	  ss.str("");
	  ss << node[i];
	  vec.push_back(LexicalCast<std::string, T>()(ss.str()));
	}
	return vec;
  }
};

template <typename T>
class LexicalCast<std::list<T>, std::string> {
 public:
  std::string operator()(const std::list<T>& v) {
	YAML::Node node;
	for (auto& e : v) {
	  node.push_back(YAML::Load(LexicalCast<T, std::string>()(e)));
	}
	std::stringstream ss;
	ss << node;
	return ss.str();
  }
};


template <typename T>
class LexicalCast<std::string, std::set<T>> {
 public:
  std::set<T> operator()(const std::string& str) {
	auto node = YAML::Load(str);
	std::set<T> vec;
	std::stringstream ss;
	for (size_t i = 0; i < node.size(); ++i) {
	  ss.str("");
	  ss << node[i];
	  vec.insert(LexicalCast<std::string, T>()(ss.str()));
	}
	return vec;
  }
};

template <typename T>
class LexicalCast<std::set<T>, std::string> {
 public:
  std::string operator()(const std::set<T>& v) {
	YAML::Node node;
	for (auto& e : v) {
	  node.push_back(YAML::Load(LexicalCast<T, std::string>()(e)));
	}
	std::stringstream ss;
	ss << node;
	return ss.str();
  }
};

template <typename T>
class LexicalCast<std::string, std::unordered_set<T>> {
 public:
  std::unordered_set<T> operator()(const std::string& str) {
	auto node = YAML::Load(str);
	std::unordered_set<T> vec;
	std::stringstream ss;
	for (size_t i = 0; i < node.size(); ++i) {
	  ss.str("");
	  ss << node[i];
	  vec.insert(LexicalCast<std::string, T>()(ss.str()));
	}
	return vec;
  }
};

template <typename T>
class LexicalCast<std::unordered_set<T>, std::string> {
 public:
  std::string operator()(const std::unordered_set<T>& v) {
	YAML::Node node;
	for (auto& e : v) {
	  node.push_back(YAML::Load(LexicalCast<T, std::string>()(e)));
	}
	std::stringstream ss;
	ss << node;
	return ss.str();
  }
};

template <typename T>
class LexicalCast<std::string, std::map<std::string, T>> {
 public:
  std::map<std::string, T> operator()(const std::string& str) {
	auto node = YAML::Load(str);
	std::map<std::string, T> vec;
	std::stringstream ss;
	for (auto it = node.begin(); it != node.end(); ++it) {
	  ss.str("");
	  ss << it->second;
	  vec.insert(
	  	std::make_pair(
	  		it->first.Scalar(),
	  		LexicalCast<std::string, T>()(ss.str()))
	  		    );
	}
	return vec;
  }
};

template <typename T>
class LexicalCast<std::map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::map<std::string, T>& v) {
	YAML::Node node;
	for (auto& e : v) {
	  node[e.first] = YAML::Load(LexicalCast<T, std::string>()(e.second));
	}
	std::stringstream ss;
	ss << node;
	return ss.str();
  }
};

template <typename T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
 public:
  std::unordered_map<std::string, T> operator()(const std::string& str) {
	auto node = YAML::Load(str);
	std::unordered_map<std::string, T> vec;
	std::stringstream ss;
	for (auto it = node.begin(); it != node.end(); ++it) {
	  ss.str("");
	  ss << it->second;
	  vec.insert(
		  std::make_pair(
			  it->first.Scalar(),
			  LexicalCast<std::string, T>()(ss.str()))
	  );
	}
	return vec;
  }
};

template <typename T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::unordered_map<std::string, T>& v) {
	YAML::Node node;
	for (auto& e : v) {
	  node[e.first] = YAML::Load(LexicalCast<T, std::string>()(e.second));
	}
	std::stringstream ss;
	ss << node;
	return ss.str();
  }
};
// FromStr T operator()(const std::string&)
// ToStr std::string opeartor()(const T&)
template<typename T, typename FromStr = LexicalCast<std::string, T>, typename ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
 public:
  using RWMutexType = RWMutex;
  using ptr = std::shared_ptr<ConfigVar>;
  using on_change_cb = std::function<void (const T& old_value, const T& new_value)>;

  ConfigVar(const std::string& name,
			const T& default_value,
			const std::string& description = "")
			: ConfigVarBase(name, description),
			m_val(default_value) {}

  std::string toString() override {
	try {
	  // return boost::lexical_cast<std::string>(m_val);
	  RWMutexType::ReadLock lock(m_mutex);
	  return ToStr()(m_val);
	} catch (std::exception& e) {
	  SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception" <<
	  e.what() << " convert: " << typeid(m_val).name() << " to string ";
	}
	return "";
  }

  bool fromString(const std::string& val) override {
	try {
	  // m_val = boost::lexical_cast<T>(val);
	  setValue(FromStr()(val));
	  return true;
	} catch (std::exception& e) {
	  SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception" <<
										e.what() << " convert: string to " << typeid(m_val).name();
	}
	return false;
  }

  const T& getValue() {
    RWMutexType::ReadLock lock(m_mutex);
    return m_val;
  }

  void setValue(const T& v) {
	{
	  RWMutexType::ReadLock lock(m_mutex);
	  if (v == m_val) {
		return;
	  }
	  // 事件回调将在数值改变时被调用
	  for (auto& i : m_cbs) {
		(i.second)(m_val, v);
	  }
	}
	RWMutexType::WriteLock lock(m_mutex);
	m_val = v;
  }

  std::string getTypeName() const override {return typeid(T).name();}

  // 添加事件监听
  uint64_t addListener(on_change_cb cb) {
    static uint64_t s_funb_id = 0;
    RWMutexType::WriteLock lock(m_mutex);
    ++s_funb_id;
    m_cbs[s_funb_id] = cb;
    return s_funb_id;
  }
  // 删除事件监听
  void delListener(uint64_t key) {
    RWMutexType::WriteLock lock(m_mutex);
    m_cbs.erase(key);
  }
  // 清空回调
  void clearListener() {
	RWMutexType::WriteLock lock(m_mutex);
	m_cbs.clear();
  }
  // 获取指定的事件回调
  on_change_cb getListener(uint64_t key) {
    RWMutexType::ReadLock lock(m_mutex);
	auto it = m_cbs.find(key);
	return it == m_cbs.end() ? nullptr : it->second;
  }
 private:
  RWMutexType m_mutex;
  T m_val;
  std::map<uint64_t, on_change_cb> m_cbs; // 变更回调函数组, key, 要求唯一,一般用hash
};

class Config {
 public:
  using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;
  using RWMutexType = RWMutex;

  template<typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name,
										   const T& default_value,
										   const std::string& description = "") {
    RWMutexType::WriteLock lock(GetMutex());
    auto it = GetDatas().find(name);
    if (it != GetDatas().end()) {
      // 存在name的配置项
      auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
	  if (tmp) {
	    // 转型成功
		SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
		return tmp;
	  } else {
	    // 类型不匹配
		SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists but type not "
		 << typeid(T).name() << " real_type= " << it->second->getTypeName()
		 << " " << it->second->toString();
		return nullptr;
	  }
	}

    // 判断名字是否合法
    if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
	  SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
      throw std::invalid_argument(name);
    }

    // 创建并插入
    auto v = std::make_shared<ConfigVar<T>>(name, default_value, description);
    GetDatas()[name] = v;
    return v;
  }

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetDatas().find(name);
    if (it == GetDatas().end()) {
	  return nullptr;
    }
    return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
  }

  static void LoadFromYAML(const YAML::Node& root);

  static ConfigVarBase::ptr LookupBase(const std::string& name);

  static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
 private:
  static ConfigVarMap& GetDatas() {
	static ConfigVarMap s_data;
	return s_data;
  }

  static RWMutexType& GetMutex() {
    static RWMutexType s_mutex;
    return s_mutex;
  }
};



}

#endif //SYLAR_SYLAR_CONFIG_H_
