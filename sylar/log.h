//
// Created by changyuli on 9/25/21.
//

#ifndef SYLAR_SYLAR_LOG_H_
#define SYLAR_SYLAR_LOG_H_

#include <cstdint>
#include <cstdarg>
#include <string>
#include <memory>
#include <list>
#include <algorithm>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

#include "util.h"
#include "singleton.h"
#include "thread.h"

#define SYLAR_LOG_LEVEL(logger, level) \
	if ((logger)->getLevel() <= (level)) \
           sylar::LogEventWarp(std::make_shared<sylar::LogEvent>(logger, level, __FILE__, __LINE__, 0, \
           sylar::GetThreadId(), sylar::GetFiberId(), time(nullptr), sylar::Thread::GetName())).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
	if (logger->getLevel() <= level) \
		sylar::LogEventWarp(std::make_shared<sylar::LogEvent>(logger, level, __FILE__, __LINE__, 0, \
           sylar::GetThreadId(), sylar::GetFiberId(), time(nullptr), sylar::Thread::GetName())).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

namespace sylar {

class Logger;

// ????????????
class LogLevel {
 public:
  enum Level {
	UNKNOW = 0,
	DEBUG = 1,
	INFO = 2,
	WARN = 3,
	ERROR = 4,
	FATAL = 5
  };

  static const char* toString(LogLevel::Level level);
  static LogLevel::Level fromString(const std::string& str);
};

class LogEvent {
 public:
  using ptr = std::shared_ptr<LogEvent>;
  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t line, uint32_t elapse, uint32_t thread_id,
		   uint32_t fiber_id, uint32_t time, std::string  thread_name);
  const char* getFile() const {return m_file;}
  int32_t getLine() const {return m_line;}
  uint32_t  getElapse() const {return m_elapse;}
  uint32_t  getThreadId() const {return m_threadId;}
  uint32_t getFiberId() const {return m_fiberId;}
  uint32_t getTime() const {return m_time;}
  std::string getContent() const {return m_ss.str();}
  std::stringstream& getSS() {return m_ss;}
  std::shared_ptr<Logger> getLogger() const {return m_logger;}
  LogLevel::Level getLevel() const {return m_level;}
  const std::string& getThreadName() const {return m_threadName;}
  void format(const char* fmt, ...);
  void format(const char* fmt, va_list list);
 private:
  const char* m_file = nullptr;  // ?????????
  int32_t m_line = 0;            // ??????
  uint32_t m_elapse = 0;         // ???????????????????????????????????????
  uint32_t m_threadId = 0;       // ????????????
  uint32_t m_fiberId = 0;        // ????????????
  uint32_t m_time;               // ?????????
  std::string m_threadName;      // ????????????
  std::stringstream m_ss;
  std::shared_ptr<Logger> m_logger;
  LogLevel::Level m_level;
};

class LogEventWarp {
 public:
  explicit LogEventWarp(LogEvent::ptr p);
  ~LogEventWarp();
  std::stringstream& getSS();
  LogEvent::ptr getEvent() {return m_event;}
 private:
  LogEvent::ptr m_event;
};

// ???????????????
class LogFormatter {
 public:
  using ptr = std::shared_ptr<LogFormatter>;
  explicit LogFormatter(const std::string& pattern);
  void init();
  bool isError() const {return m_error;}
  const std::string& getPattern() const {return m_pattern;}

  // %t \t  %threadId %m %n
  std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

 public:
  class FormatItem {
   public:
	using ptr = std::shared_ptr<FormatItem>;
	explicit FormatItem(const std::string& fmt = "") {}
    virtual ~FormatItem() = default;
	virtual void format(std::ostream& out, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
  };

 private:
  std::string m_pattern;
  std::vector<FormatItem::ptr> m_items;
  bool m_error = false;
};


// ???????????????
class LogAppender {
  friend class Logger;
 public:
  using ptr = std::shared_ptr<LogAppender>;
  using MutexType = SpinLock;
  virtual ~LogAppender() = default;

  virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
  virtual std::string toYamlString() = 0;

  void setFormatter(LogFormatter::ptr val);
  LogFormatter::ptr getFormatter();
  LogLevel::Level getLevel() const {return m_level;}
  void setLevel(LogLevel::Level level) {m_level = level;}
 protected:
  LogLevel::Level m_level = LogLevel::DEBUG;
  LogFormatter::ptr m_formatter;
  bool m_hasFormatter = false;
  MutexType m_mutex;
};

// ?????????
 class Logger : public std::enable_shared_from_this<Logger> {
   friend class LoggerManager;
 public:
  using ptr = std::shared_ptr<Logger>;
  using MutexType = SpinLock;

  explicit Logger(std::string  name = "root");

  void log(LogLevel::Level level, const LogEvent::ptr& event);

  void debug(LogEvent::ptr event);
  void info(LogEvent::ptr event);
  void warn(LogEvent::ptr event);
  void error(LogEvent::ptr event);
  void fatal(LogEvent::ptr event);

  void addAppender(LogAppender::ptr appender);
  void deleteAppender(LogAppender::ptr appender);
  void clearAppender();
  LogLevel::Level getLevel() const {return m_level;}
  void setLevel(LogLevel::Level level) {m_level = level;}
  std::string getName() const {return m_name;}

  void setFormatter(LogFormatter::ptr val);
  void setFormatter(const std::string& val);
  LogFormatter::ptr getFormatter();

  std::string toYamlString();

 private:
  std::string m_name;          // ????????????
  LogLevel::Level m_level;     // ????????????
  MutexType m_mutex;
  std::list<LogAppender::ptr> m_appenders; // Appender??????
  LogFormatter::ptr m_logformatter;
  Logger::ptr m_root;
};

// ?????????????????????appender
class StdoutLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<StdoutLogAppender>;
  void log(std::shared_ptr<Logger> logger, LogLevel::Level level, const LogEvent::ptr event) override;
  std::string toYamlString() override;
};

// ??????????????????appender
class FileLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<FileLogAppender>;
  explicit FileLogAppender(const std::string& filename);
  void log(std::shared_ptr<Logger> logger, LogLevel::Level level, const LogEvent::ptr event) override;

  std::string toYamlString() override;
  // ??????????????????,??????????????????true
  bool reopen();
 private:
  std::string m_filename;
  std::ofstream m_fileoutstream;
};

class LoggerManager {
 public:
  using MutexType = SpinLock;
  LoggerManager();
  Logger::ptr getLogger(const std::string& name);
  void init();
  Logger::ptr getRoot() const {return m_root;}
  std::string toYamlString();
 private:
  MutexType m_mutex;
  std::map<std::string, Logger::ptr> m_loggers;
  Logger::ptr m_root;
};

using LoggerMgr = sylar::SingletonPtr<LoggerManager>;

}
#endif //SYLAR_SYLAR_LOG_H_
