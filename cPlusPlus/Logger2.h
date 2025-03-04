#ifndef LOGGER2_H
#define LOGGER2_H

#include <iostream>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <queue>
#include <string>
#include <iomanip>
#include <bitset>
#include <vector>
#include <filesystem>
#include <thread>
#include <atomic>
#include <memory>
#include <sstream>
#include <cstring>
#include <zlib.h>       // 用于 gzip 压缩
#include <sys/socket.h> // 用于 TCP 远程日志
#include <netinet/in.h> // 用于 TCP 远程日志
#include <arpa/inet.h>  // 用于 TCP 远程日志
#include <syslog.h>     // 用于 syslog 支持

#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

// 日志等级枚举
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
    // 获取单例实例
    static Logger& getInstance(const std::string& path = "/tmp/logs", const std::string& name = "app_log",
                               size_t maxFileSize = 1024 * 1024, size_t maxFileCount = 5);

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 日志记录方法
    void log(LogLevel_en level, const std::string& format, const char* file, int line, ...);

    // 修改配置方法
    void setLogPath(const std::string& path);
    void setMaxFileSize(size_t maxFileSize);
    void setMaxFileCount(size_t maxFileCount);
    void setLogName(const std::string& name);

    // 打印等级控制方法
    void enableLogLevel(LogLevel_en level, bool enable);
    void enableLogLevelAbove(LogLevel_en level, bool enable);

    // 控制是否输出到终端
    void setOutputToConsole(bool enable);

    // 压缩日志开关
    void enableLogCompression(bool enable);
    void setMaxCompressedFiles(size_t maxCompressedFiles);
    void setMaxCompressedFileSize(size_t maxCompressedFileSize);

    // JSON 格式日志开关
    void setJsonFormat(bool enable);

    // 远程日志配置
    void enableRemoteLogging(const std::string& remoteIp, uint16_t remotePort);
    void enableSyslog(const std::string& ident, int facility, int syslogLevel);

    // 析构函数
    ~Logger();

private:
    // 构造函数
    Logger(const std::string& path, const std::string& name, size_t maxFileSize = 1024 * 1024, size_t maxFileCount = 5);

    // 检查并创建日志目录
    void checkAndCreateLogDirectory();

    // 获取日志等级字符串
    std::string getLogLevelString(LogLevel_en level);

    // 检查文件大小并触发日志滚动
    void checkFileSize();

    // 日志滚动
    void rotateLogs();

    // 压缩文件
    void compressFile(const fs::path& filePath);

    // 获取当前时间字符串
    std::string getCurrentTimeString();

    // 日志写入线程函数
    void writeThreadFunc();

    // 写入日志到文件
    void writeToFile(const std::string& message);

    // 写入日志到远程服务器
    void writeToRemote(const std::string& message);

    // 写入日志到 syslog
    void writeToSyslog(LogLevel_en level, const std::string& message);

    // 成员变量
    std::mutex mutex;                      // 互斥锁
    std::condition_variable cv;            // 条件变量
    std::queue<std::string> logQueue;      // 日志缓存队列
    std::atomic<bool> running;             // 控制写线程运行状态
    std::thread writeThread;               // 日志写入线程

    fs::path logPath;                      // 日志文件路径
    std::string logName;                   // 日志文件名
    fs::path currentFilePath;              // 当前日志文件路径
    size_t maxFileSize;                    // 单个日志文件最大大小
    size_t maxFileCount;                   // 最大日志文件数量
    std::ofstream outFile;                 // 文件输出流

    std::bitset<5> logLevelEnabled = 0b11111; // 日志等级启用状态

    bool outputToConsole = false;          // 是否输出到终端
    bool compressLogs = false;             // 是否压缩日志
    bool jsonFormat = false;               // 是否使用 JSON 格式

    // 远程日志配置
    std::string remoteIp;                  // 远程服务器 IP
    uint16_t remotePort = 0;               // 远程服务器端口
    int sockfd = -1;                       // TCP 套接字
    bool useSyslog = false;                // 是否使用 syslog
    std::string syslogIdent;               // syslog 标识
    int syslogFacility = LOG_USER;         // syslog 设施
    int syslogLevel = LOG_DEBUG;           // syslog 级别

    size_t maxCompressedFiles = 10;        // 最大压缩文件数量
    size_t maxCompressedFileSize = 1024 * 1024; // 单个压缩文件最大大小
    std::vector<fs::path> compressedFiles; // 压缩文件路径列表
};

#endif // LOGGER2_H