#ifndef SIMPLE_FILE_SYSTEM_H
#define SIMPLE_FILE_SYSTEM_H

#include <iostream>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <queue>
#include <string>
#include <iomanip>
#include <map>
#include <cstdarg>  // 包含 va_list 和相关函数

class Logger;


#define LOG(level, format, ...) Logger::getInstance().log(level, format, __FILE__, __LINE__, ##__VA_ARGS__)



#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

enum LogLevel_en
{
    DEBUG = 1,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger
{
public:
    static Logger& getInstance(const std::string& path = "/tmp/logs", const std::string& name = "app_log",
                                size_t maxFileSize = 1024 * 1024, size_t maxFileCount = 5);

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 日志记录方法
    void log(LogLevel_en level, const std::string& format, const char* file, int line, ...);
    void log(LogLevel_en level, const std::string& format, ...);

private:
    Logger(const std::string& path, const std::string& name, size_t maxFileSize = 1024 * 1024, size_t maxFileCount = 5);
    std::string getLogLevelString(LogLevel_en level);
    void checkFileSize();
    void rotateLogs();
    std::string getCurrentTimeString();

    std::mutex mutex;
    fs::path logPath;
    std::string logName;
    fs::path currentFilePath;
    size_t maxFileSize;
    size_t maxFileCount;
    std::ofstream outFile;
    std::map<LogLevel_en, std::string> logLevelMap = {
        {DEBUG, "DEBUG"}, {INFO, "INFO"}, {WARNING, "WARNING"},
        {ERROR, "ERROR"}, {FATAL, "FATAL"}
    };
};


#endif // SIMPLE_FILE_SYSTEM_H