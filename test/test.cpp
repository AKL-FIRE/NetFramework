//
// Created by changyuli on 9/25/21.
//

#include "../sylar/log.h"

#include <memory>
#include <thread>

using namespace std;
using namespace sylar;

int main() {
  auto logger = std::make_shared<Logger>();
  logger->addAppender(std::make_shared<StdoutLogAppender>());
  auto file_appender = std::make_shared<FileLogAppender>("./log.txt");
  file_appender->setLevel(LogLevel::ERROR);
  file_appender->setFormatter(std::make_shared<LogFormatter>("%d%T%p%T%m\n"));
  logger->addAppender(file_appender);
  SYLAR_LOG_DEBUG(logger) << "HAHAHA";
  SYLAR_LOG_ERROR(logger) << "This is an error";
  SYLAR_LOG_FMT_ERROR(logger, "test for error %s", "bebug");

  auto ins = LoggerMgr::GetInstance();
  SYLAR_LOG_DEBUG(ins->getLogger("xx")) << "WUWUW";

  return 0;
}