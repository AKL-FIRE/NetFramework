//
// Created by changyuli on 9/25/21.
//

#include "log.h"
#include "config.h"

#include <map>
#include <utility>
#include <time.h>


namespace sylar{


class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  explicit MessageFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << event->getContent();
  }
};

class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LevelFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << LogLevel::toString(level);
  }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ElapseFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << event->getElapse();
  }
};

class NameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit NameFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	// out << logger->getName();
	out << event->getLogger()->getName();
  }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit ThreadIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << event->getThreadId();
  }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
 public:
  explicit FiberIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << event->getFiberId();
  }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  explicit DateTimeFormatItem(std::string format = "%Y-%m-%d %H:%M:%S")
	  : m_format(std::move(format)) {
    if (m_format.empty()) {
      m_format = "%Y-%m-%d %H:%M:%S";
    }
  }

  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	tm tmp {};
	time_t t = event->getTime();
	localtime_r(&t, &tmp);
	char buf[64];
	strftime(buf, sizeof(buf), m_format.c_str(), &tmp);
    out << buf;
  }
 private:
  std::string m_format;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
 public:
  explicit FileNameFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << event->getFile();
  }
};

class LineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit LineFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << event->getLine();
  }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  explicit NewLineFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << std::endl;
  }
};

class TabFormatItem : public LogFormatter::FormatItem {
 public:
  explicit TabFormatItem(const std::string& str = "") {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << "\t";
  }
};

class StringFormatItem : public LogFormatter::FormatItem {
 public:
  explicit StringFormatItem(const std::string& str)
  	:FormatItem(str), m_string(str) {}
  void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
	out << m_string;
  }
 private:
  std::string m_string;
};

Logger::Logger(std::string name) : m_name (std::move(name)), m_level(LogLevel::DEBUG) {
  m_logformatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T[%p]%T[%c]%T%t%T%F%T%f:%l%T%m\n"));
}

void Logger::log(LogLevel::Level level, const LogEvent::ptr& event) {
  if (level >= m_level) {
	auto self = shared_from_this();
	MutexType::Lock lock(m_mutex);
	if (!m_appenders.empty()) {
	  for (auto& c : m_appenders) {
		c->log(self, level, event);
	  }
	} else if (m_root) {
	  m_root->log(level, event);
	}
  }
}

void Logger::debug(LogEvent::ptr event) {
  // debug(LogLevel::DEBUG, event);
  log(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event) {
  log(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
  log(LogLevel::WARN, event);
}

void Logger::error(LogEvent::ptr event) {
  log(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event) {
  log(LogLevel::FATAL, event);
}

void Logger::addAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(m_mutex);
   if (!appender->getFormatter()) {
     MutexType::Lock li(appender->m_mutex);
     appender->m_formatter = m_logformatter;
   }
  m_appenders.push_back(appender);
}

void Logger::deleteAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(m_mutex);
  for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
    if (*it == appender) {
      m_appenders.erase(it);
	  break;
    }
  }
}

void Logger::clearAppender() {
  MutexType::Lock lock(m_mutex);
  m_appenders.clear();
}

void Logger::setFormatter(LogFormatter::ptr val) {
  MutexType::Lock lock(m_mutex);
  m_logformatter = std::move(val);

  for (auto& i : m_appenders) {
    MutexType::Lock li(i->m_mutex);
    if (!i->m_hasFormatter) {
      i->m_formatter = m_logformatter;
    }
  }
}

void Logger::setFormatter(const std::string& val) {
  auto new_val = std::make_shared<LogFormatter>(val);
  if (new_val->isError()) {
    std::cout << "Logger setFormatter name=" << m_name << " value=" << val << " invalid formatter" << std::endl;
	return;
  }
  // m_logformatter = new_val;
  setFormatter(new_val);
}

LogFormatter::ptr Logger::getFormatter() {
  MutexType::Lock lock(m_mutex);
  return m_logformatter;
}

std::string Logger::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["name"] = m_name;
  if (m_level != LogLevel::UNKNOW)
  	node["level"] = LogLevel::toString(m_level);
  if (!m_logformatter) {
    node["formatter"] = m_logformatter->getPattern();
  }

  for(auto& i : m_appenders) {
    node["appenders"].push_back(YAML::Load(i->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

FileLogAppender::FileLogAppender(const std::string &filename)
	: m_filename(filename) {
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, const LogEvent::ptr event) {
  if (level >= m_level) {
    if (!m_fileoutstream.is_open()) {
      if (reopen()) {
		MutexType::Lock lock(m_mutex);
		m_fileoutstream << m_formatter->format(logger, level, event);
	  } else {
		SYLAR_LOG_FMT_FATAL(logger, "Can't open file %s", m_filename.c_str());
		std::exit(-1);
      }
    } else {
	  MutexType::Lock lock(m_mutex);
	  m_fileoutstream << m_formatter->format(logger, level, event);
    }
  }
}

bool FileLogAppender::reopen() {
  MutexType::Lock lock(m_mutex);
  if (m_fileoutstream.is_open()) {
    m_fileoutstream.close();
  }
  m_fileoutstream.open(m_filename);
  return m_fileoutstream.is_open();
}

std::string FileLogAppender::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["type"] = "FileLogAppender";
  node["file"] = m_filename;
  if (m_level != LogLevel::UNKNOW)
	node["level"] = LogLevel::toString(m_level);
  if (m_formatter && m_hasFormatter) {
    node["formatter"] = m_formatter->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, const LogEvent::ptr event) {
  if (level >= m_level) {
	MutexType::Lock lock(m_mutex);
	std::cout << m_formatter->format(logger, level, event);
  }
}

std::string StdoutLogAppender::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["type"] = "StdoutLogAppender";
  if (m_level != LogLevel::UNKNOW)
	node["level"] = LogLevel::toString(m_level);
  if (m_formatter && m_hasFormatter) {
	node["formatter"] = m_formatter->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogFormatter::LogFormatter(const std::string &pattern)
	:m_pattern(pattern){
  init();
}

// %xxx %xxx{xxx} %%
void LogFormatter::init() {
  // str, format, type
  std::vector<std::tuple<std::string, std::string, int>> vec;
  std::string str;
  for (size_t i = 0; i < m_pattern.size(); ++i) {
    if (m_pattern[i] != '%') {
      str.append(1, m_pattern[i]);
	  continue;
    }

    if ((i + 1) < m_pattern.size()) {
      if (m_pattern[i + 1] == '%') {
        str.append(1, '%');
		continue;
      }
    }

    size_t n = i + 1;
    // 0: 非解析模式
    // 1: 解析模式
    int fmt_status = 0;
    size_t fmt_begin = 0; // 指向'{'
    std::string tmp;
    std::string fmt;
    while (n < m_pattern.size()) {
      if (fmt_status == 0 && (!std::isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')) {
        tmp = m_pattern.substr(i + 1, n - i -1);
		break;
      }
      if (fmt_status == 0) {
        if (m_pattern[n] == '{') {
          tmp = m_pattern.substr(i + 1, n - i - 1);
          fmt_status = 1; // 开始解析{}内参数
		  fmt_begin = n;
		  n++;
		  continue;
        }
      } else if (fmt_status == 1) {
        if (m_pattern[n] == '}') {
          // 此时{}内参数解析完毕
          fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
          fmt_status = 0;
          n++;
		  break;
        }
      }
      n++;
      if (n == m_pattern.size()) {
        // 处理最后一位的边界条件
        if (tmp.empty()) {
          tmp = m_pattern.substr(i + 1);
        }
      }
    }
    if (fmt_status == 0) {
      if (!str.empty()) {
        vec.emplace_back(str, "", 0);
        str.clear();
      }
      vec.emplace_back(tmp, fmt, 1);
      i = n - 1;
    } else if (fmt_status == 1) {
      std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
      vec.emplace_back("<<pattern_error>>", fmt, 0);
      m_error = true;
    }
  }
  if (!str.empty()) {
    vec.emplace_back(str, "", 0);
  }
  static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)>> s_format_items = {
#define XX(str, C) \
	  {#str, [](const std::string& fmt) {  \
	    return std::static_pointer_cast<FormatItem>(std::make_shared<C>(fmt)); \
	  }}
	  XX(m, MessageFormatItem),
	  XX(p, LevelFormatItem),
	  XX(r, ElapseFormatItem),
	  XX(c, NameFormatItem),
	  XX(t, ThreadIdFormatItem),
	  XX(n, NewLineFormatItem),
	  XX(d, DateTimeFormatItem),
	  XX(f, FileNameFormatItem),
	  XX(l, LineFormatItem),
	  XX(T, TabFormatItem),
	  XX(F, FiberIdFormatItem)
#undef XX
  };

  for (auto& e : vec) {
    if (std::get<2>(e) == 0) {
      m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(e))));
    } else {
      auto it = s_format_items.find(std::get<0>(e));
      if (it == s_format_items.end()) {
        m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" +
        std::get<0>(e) + ">>")));
        m_error = true;
      } else {
        m_items.push_back(it->second(std::get<1>(e)));
      }
    }
    // std::cout << "(" <<std::get<0>(e) << ") - (" << std::get<1>(e) << ") - (" << std::get<2>(e) << ")" << std::endl;
  }
  /*
   * %m : 消息体
   * %p : 消息等级
   * %r : 程序启动后到现在的时间
   * %c : 日志名称
   * %t : 线程id
   * %n : 回车换行
   * %d : 时间
   * %f : 文件名
   * %l : 行号
   * */
}


std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
  std::stringstream  ss;
  for(auto& c : m_items) {
    c->format(ss, logger, level, event);
  }
  return ss.str();
}

const char *LogLevel::toString(LogLevel::Level level) {
  switch (level) {
#define xx(name) \
    case LogLevel::name: \
      return #name; \
      break;

    xx(DEBUG)
    xx(INFO)
    xx(WARN)
    xx(ERROR)
    xx(FATAL)
#undef xx
    default:
      return "UNKNOW";
  }
  return "UNKNOW";
}

LogLevel::Level LogLevel::fromString(const std::string& str) {
  std::string str1 = str;
  std::transform(str1.begin(), str1.end(), str1.begin(), ::toupper);
#define XX(name) \
  if (str1 == #name) { \
    return LogLevel::name; \
  }
  XX(DEBUG)
  XX(INFO)
  XX(WARN)
  XX(ERROR)
  XX(FATAL)
#undef XX
	return LogLevel::UNKNOW;
}

LogEvent::LogEvent(std::shared_ptr<Logger> logger,
				   LogLevel::Level level,
				   const char *file,
				   int32_t line,
				   uint32_t elapse,
				   uint32_t thread_id,
				   uint32_t fiber_id,
				   uint32_t time) :
				   m_file(file) ,
				   m_line(line),
				   m_elapse(elapse),
				   m_threadId(thread_id),
				   m_fiberId(fiber_id),
				   m_time(time),
				   m_logger(std::move(logger)),
				   m_level(level) {
}

void LogEvent::format(const char *fmt, ...) {
  va_list al;
  va_start(al, fmt);
  format(fmt, al);
  va_end(al);
}

void LogEvent::format(const char *fmt, va_list list) {
  char* buf = nullptr;
  int len = vasprintf(&buf, fmt, list);
  if (len != -1) {
    m_ss << std::string(buf, len);
	free(buf);
  }
}

LogEventWarp::LogEventWarp(LogEvent::ptr p)
	: m_event(std::move(p)){
}

LogEventWarp::~LogEventWarp() {
  m_event->getLogger()->log(m_event->getLevel(), m_event);
}

std::stringstream &LogEventWarp::getSS() {
  return m_event->getSS();
}

LoggerManager::LoggerManager() {
  m_root.reset(new Logger);
  m_root->addAppender(std::make_shared<StdoutLogAppender>());

  m_loggers[m_root->m_name] = m_root;
  init();
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
  MutexType::Lock lock(m_mutex);
  auto it = m_loggers.find(name);
  if (it != m_loggers.end()) {
    // 存在就返回
    return it->second;
  }
  // 不存在就创建一个
  auto logger = std::make_shared<Logger>(name);
  logger->m_root = m_root;
  m_loggers[name] = logger;
  return logger;
}

struct LogAppenderDefine {
  int type = 0; // 1 File, 2 Stdout
  LogLevel::Level level = LogLevel::UNKNOW;
  std::string formatter;
  std::string file;

  bool operator==(const LogAppenderDefine& other) const {
    return type == other.type &&
    	level == other.level &&
    	formatter == other.formatter &&
    	file == other.file;
  }
};

struct LogDefine {
  std::string name;
  LogLevel::Level level = LogLevel::UNKNOW;
  std::string formatter;
  std::vector<LogAppenderDefine> appenders;

  bool operator==(const LogDefine& other) const {
    return name == other.name &&
    	level == other.level &&
    	formatter == other.formatter &&
    	appenders == other.appenders;
  }

  bool operator<(const LogDefine& other) const {
    return name < other.name;
  }
};

template <>
class LexicalCast<std::string, std::set<LogDefine>> {
 public:
  std::set<LogDefine> operator()(const std::string& str) {
	auto node = YAML::Load(str);
	std::set<LogDefine> vec;
	for (size_t i = 0; i < node.size(); ++i) {
	  auto n = node[i];
	  if (!n["name"].IsDefined()) {
	    std::cout << "log config error: name is null, " << n << std::endl;
		continue;
	  }
	  LogDefine ld;
	  ld.name = n["name"].as<std::string>();
	  ld.level = LogLevel::fromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
	  if (n["formatter"].IsDefined()) {
		ld.formatter = n["formatter"].as<std::string>();
	  }
	  if (n["appenders"].IsDefined()) {
	    for (size_t x = 0; x < n["appenders"].size(); ++x) {
	      auto a = n["appenders"][x];
	      if (!a["type"].IsDefined()) {
	        std::cout << "log config error: appender type is null, " <<
	        a << std::endl;
			continue;
	      }
	      auto type = a["type"].as<std::string>();
	      LogAppenderDefine lad;
	      if (type == "FileLogAppender") {
	        lad.type = 1;
	        if (!a["file"].IsDefined()) {
			  std::cout << "log config error: file appender's file is null, " <<
						a << std::endl;
			  continue;
			}
	        lad.file = a["file"].as<std::string>();
	        if (a["formatter"].IsDefined()) {
	          lad.formatter = a["formatter"].as<std::string>();
	        }
	      } else if (type == "StdoutLogAppender") {
	        lad.type = 2;
	      } else {
			std::cout << "log config error: appender type is null, " <<
					  a << std::endl;
			continue;
		  }
	      ld.appenders.push_back(lad);
		}
	  }
	  vec.insert(ld);
	}
	return vec;
  }
};

template <>
class LexicalCast<std::set<LogDefine>, std::string> {
 public:
  std::string operator()(const std::set<LogDefine>& v) {
	YAML::Node node;
	for (auto& e : v) {
	  YAML::Node n;
	  n["name"] = e.name;
	  if (e.level != LogLevel::UNKNOW)
	  	n["level"] = LogLevel::toString(e.level);
	  if(!e.formatter.empty()) {
	    n["formatter"] = e.formatter;
	  }

	  for (auto& i : e.appenders) {
	    YAML::Node node1;
	    if (i.type == 1) {
	      node1["type"] = "FileLogAppender";
	      node1["file"] = i.file;
	    } else if (i.type == 2) {
	      node1["type"] = "StdoutLogAppender";
	    }
	    if (i.level != LogLevel::UNKNOW)
		  node1["level"] = LogLevel::toString(i.level);
	    if (!i.formatter.empty()) {
	      node1["formatter"] = i.formatter;
	    }
	    n["appenders"].push_back(node1);
	  }
	  node.push_back(n);
	}
	std::stringstream ss;
	ss << node;
	return ss.str();
  }
};

auto g_log_defines = sylar::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
  LogIniter() {
    g_log_defines->addListener([](const std::set<LogDefine>& old_value, const std::set<LogDefine>& new_value) {
      // 新增的日志
	  SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "on_logger_conf_changed";
      for (auto& i : new_value) {
        auto it = old_value.find(i);
        Logger::ptr logger;
        if (it == old_value.end()) {
          // 新增logger(新的有，旧的没有)
          logger = SYLAR_LOG_NAME(i.name);
		} else {
          if (!(i == *it)) {
            // 修改的logger
            // 先找到logger
            logger = SYLAR_LOG_NAME(i.name);
          }
        }
		logger->setLevel(i.level);
		if (!i.formatter.empty()) {
		  logger->setFormatter(i.formatter);
		}

		logger->clearAppender();
		for (auto& a : i.appenders) {
		  LogAppender::ptr ap;
		  switch (a.type) {
			case 1:
			  ap.reset(new FileLogAppender(a.file));
			  break;
			case 2:
			  ap.reset(new StdoutLogAppender());
			  break;
		  }
		  ap->setLevel(a.level);
		  if (!a.formatter.empty()) {
		    auto fmt = std::make_shared<LogFormatter>(a.formatter);
		    if (!fmt->isError()) {
		      ap->setFormatter(fmt);
		    } else {
		      std::cout << "log.name=" << i.name << " appender type=" << a.type << " formatter=" << a.formatter << " is valid" << std::endl;
		    }
		  }
		  logger->addAppender(ap);
		}
	  }
	  for (auto& i : old_value) {
		auto it = new_value.find(i);
		if (it == new_value.end()) {
		  // 删除logger(旧的有，新的没有)
		  auto logger = SYLAR_LOG_NAME(i.name);
		  logger->setLevel(LogLevel::Level(100));
		  logger->clearAppender();
      	}
      }
    });
  }
};

static LogIniter __log_init; // 静态全局变量将在main函数之前构造完成

void LoggerManager::init() {

}

std::string LoggerManager::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  for (auto& i : m_loggers) {
    node.push_back(YAML::Load(i.second->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void LogAppender::setFormatter(LogFormatter::ptr val) {
  MutexType::Lock lock(m_mutex);
  m_formatter = std::move(val);
  m_hasFormatter = m_formatter ? true : false;
}

LogFormatter::ptr LogAppender::getFormatter() {
  MutexType::Lock lock(m_mutex);
  return m_formatter;
}

}