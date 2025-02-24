#ifndef LOG_H
#define LOG_H

#include "Logger.h"


#define LOG_INIT(path, name, maxFileSize, maxFileCount) \
    Logger::getInstance(path, name, maxFileSize, maxFileCount)

#define LOG(level, format, ...) \
    Logger::getInstance().log(level, format, __FILE__, __LINE__, ##__VA_ARGS__)


#endif // LOG_H