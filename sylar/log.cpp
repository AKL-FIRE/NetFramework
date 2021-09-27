//
// Created by changyuli on 9/25/21.
//

#include <map>
#include <utility>
#include <time.h>
#include "log.h"

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
	out << logger->getName();
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

void Logger::log(LogLevel::Level level, const LogEvent::ptr event) {
  if (level >= m_level) {
	auto self = shared_from_this();
	for (auto& c : m_appenders) {
      c->log(self, level, event);
    }
  }
}

void Logger::debug(LogEvent::ptr event) {
  // debug(LogLevel::DEBUG, event);
  log(LogLevel::DEBUG, std::move(event));
}

void Logger::info(LogEvent::ptr event) {
  log(LogLevel::INFO, std::move(event));
}

void Logger::warn(LogEvent::ptr event) {
  log(LogLevel::WARN, std::move(event));
}

void Logger::error(LogEvent::ptr event) {
  log(LogLevel::ERROR, std::move(event));
}

void Logger::fatal(LogEvent::ptr event) {
  log(LogLevel::FATAL, std::move(event));
}

void Logger::addAppender(LogAppender::ptr appender) {
  if (!appender->getFormatter()) {
    appender->setFormatter(m_logformatter);
  }
  m_appenders.push_back(appender);
}

void Logger::deleteAppender(LogAppender::ptr appender) {
  auto it = std::find(m_appenders.begin(), m_appenders.end(), appender);
  if (it != m_appenders.end()) {
    m_appenders.erase(it);
  }
}

FileLogAppender::FileLogAppender(const std::string &filename)
	: m_filename(filename) {
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, const LogEvent::ptr event) {
  if (level >= m_level) {
    if (!m_fileoutstream.is_open()) {
      if (reopen()) {
		m_fileoutstream << m_formatter->format(logger, level, event);
	  } else {
		SYLAR_LOG_FMT_FATAL(logger, "Can't open file %s", m_filename.c_str());
		std::exit(-1);
      }
    } else {
	  m_fileoutstream << m_formatter->format(logger, level, event);
    }
  }
}

bool FileLogAppender::reopen() {
  if (m_fileoutstream.is_open()) {
    m_fileoutstream.close();
  }
  m_fileoutstream.open(m_filename);
  return m_fileoutstream.is_open();
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, const LogEvent::ptr event) {
  if (level >= m_level) {
    std::cout << m_formatter->format(logger, level, event);
  }
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
          fmt_status = 1;
		  fmt_begin = n;
		  n++;
		  continue;
        }
      } else if (fmt_status == 1) {
        if (m_pattern[n] == '}') {
          fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
          fmt_status = 0;
          n++;
		  break;
        }
      }
      n++;
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
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
  auto it = m_loggers.find(name);
  return it == m_loggers.end() ? m_root : it->second;
}

void LoggerManager::init() {

}

}