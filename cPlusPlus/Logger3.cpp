#include "Logger3.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <sys/file.h> // 文件锁

// 构造函数
// Logger::Logger(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount)
//     : logPath(path), logName(name), maxFileSize(maxFileSize), maxFileCount(maxFileCount), running(true) {
//     if (maxFileCount < 1) {
//         throw std::invalid_argument("maxFileCount must be at least 1");
//     }

//     checkAndCreateLogDirectory(); // 检查并创建日志目录
//     currentFilePath = logPath / (logName + "_1.log"); // 当前日志文件路径

//     outFile.open(currentFilePath, std::ios::app); // 打开日志文件
//     if (!outFile) {
//         throw std::runtime_error("Failed to open log file: " + currentFilePath.string());
//     }

//     writeThread = std::thread(&Logger::writeThreadFunc, this); // 启动日志写入线程
//     compressThread = std::thread(&Logger::compressThreadFunc, this); // 启动压缩线程
//     remoteThread = std::thread(&Logger::remoteThreadFunc, this); // 启动远程日志线程
// }

// 构造函数
Logger::Logger(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount)
    : logPath(path), logName(name), maxFileSize(maxFileSize), maxFileCount(maxFileCount), running(true) {
    if (maxFileCount < 1) {
        throw std::invalid_argument("maxFileCount must be at least 1");
    }

    checkAndCreateLogDirectory(); // 检查并创建日志目录
    currentFilePath = logPath / (logName + "_1.log"); // 当前日志文件路径

    // 添加调试信息
    if (!fs::exists(currentFilePath)) {
        std::cerr << "Warning: Current log file does not exist on creation: " << currentFilePath.string() << std::endl;
    }

    outFile.open(currentFilePath, std::ios::app); // 打开日志文件
    if (!outFile) {
        throw std::runtime_error("Failed to open log file: " + currentFilePath.string());
    }

    writeThread = std::thread(&Logger::writeThreadFunc, this); // 启动日志写入线程
    compressThread = std::thread(&Logger::compressThreadFunc, this); // 启动压缩线程
    remoteThread = std::thread(&Logger::remoteThreadFunc, this); // 启动远程日志线程
}

// 析构函数
Logger::~Logger() {
    running = false;
    compressRunning = false;
    remoteRunning = false;
    cv.notify_all();
    compressCV.notify_all();

    if (writeThread.joinable()) writeThread.join();
    if (compressThread.joinable()) compressThread.join();
    if (remoteThread.joinable()) remoteThread.join();

    // 处理剩余日志
    while (!logQueue.empty()) {
        writeToFile(logQueue.front());
        logQueue.pop();
    }

    if (sockfd != -1) close(sockfd); // 关闭 TCP 套接字
    if (useSyslog) closelog(); // 关闭 syslog
}

// 获取单例实例
Logger& Logger::getInstance(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount) {
    static Logger instance(path, name, maxFileSize, maxFileCount);
    return instance;
}


void Logger::checkAndCreateLogDirectory() {
    std::error_code ec;
    if (!fs::exists(logPath)) {
        if (!fs::create_directories(logPath, ec)) {
            throw std::runtime_error("Failed to create log directory: " + logPath.string());
        }
    } else if (ec) {
        throw std::runtime_error("Error accessing log directory: " + logPath.string());
    }

    currentFilePath = logPath / (logName + "_1.log");
    if (!fs::exists(currentFilePath)) {
        std::ofstream file(currentFilePath);
        if (!file) {
            throw std::runtime_error("Failed to create log file: " + currentFilePath.string());
        }
        file.close();
    }
}
// 日志记录方法
void Logger::log(LogLevel_en level, const std::string& format, const char* file, int line, ...) {
    if (!logLevelEnabled.test(level - 1)) return; // 如果该日志等级未启用，直接返回

    va_list args;
    va_start(args, line);
    int len = vsnprintf(nullptr, 0, format.c_str(), args);
    va_end(args);
    std::vector<char> buffer(len + 1);
    va_start(args, line);
    vsnprintf(buffer.data(), buffer.size(), format.c_str(), args);
    va_end(args);
    std::string message(buffer.data());

    std::ostringstream logEntry;
    if (jsonFormat) { // JSON 格式日志
        logEntry << "{"
                 << "\"timestamp\":\"" << getCurrentTimeString() << "\","
                 << "\"level\":\"" << getLogLevelString(level) << "\","
                 << "\"file\":\"" << (file ? file : "unknown") << "\","
                 << "\"line\":" << line << ","
                 << "\"message\":\"" << message << "\""
                 << "}";
    } else { // 纯文本格式日志
        logEntry << "[" << getCurrentTimeString() << "]"
                 << "[" << getLogLevelString(level) << "]"
                 << "[" << (file ? file : "unknown") << ":" << line << "] "
                 << message;
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (logQueue.size() >= maxQueueSize) {
        logQueue.pop(); // 丢弃最旧日志
    }
    logQueue.push(logEntry.str()); // 将日志消息加入队列
    cv.notify_one(); // 通知写线程

    if (outputToConsole) { // 输出到终端
        std::cout << logEntry.str() << std::endl;
    }

    if (useSyslog && syslogLevel <= level) { // 输出到 syslog
        writeToSyslog(level, message);
    }

    if (!remoteIp.empty() && remotePort != 0) { // 输出到远程服务器
        remoteQueue.push(logEntry.str());
    }
}

// 日志写入线程函数
void Logger::writeThreadFunc() {
    while (running || !logQueue.empty()) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !logQueue.empty() || !running; });

        while (!logQueue.empty()) {
            std::string message = logQueue.front();
            logQueue.pop();
            lock.unlock();

            writeToFile(message); // 写入日志到文件

            lock.lock();
        }
    }
}



// 压缩线程函数
void Logger::compressThreadFunc() {
    while (compressRunning || !compressQueue.empty()) {
        std::unique_lock<std::mutex> lock(compressMutex);
        compressCV.wait(lock, [this] { return !compressQueue.empty() || !compressRunning; });

        if (!compressQueue.empty()) {
            auto task = std::move(compressQueue.front());
            compressQueue.pop();
            lock.unlock();
            task(); // 执行压缩任务
        }
    }
}

// 远程日志线程函数
void Logger::remoteThreadFunc() {
    while (remoteRunning || !remoteQueue.empty()) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!remoteQueue.empty()) {
            std::string msg = std::move(remoteQueue.front());
            remoteQueue.pop();
            lock.unlock();

            if (sockfd == -1) {
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0) {
                    std::cerr << "Failed to create socket for remote logging" << std::endl;
                    continue;
                }

                struct sockaddr_in serverAddr;
                memset(&serverAddr, 0, sizeof(serverAddr));
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(remotePort);
                inet_pton(AF_INET, remoteIp.c_str(), &serverAddr.sin_addr);

                if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
                    std::cerr << "Failed to connect to remote server" << std::endl;
                    close(sockfd);
                    sockfd = -1;
                    continue;
                }
            }

            if (send(sockfd, msg.c_str(), msg.size(), MSG_DONTWAIT) == -1) {
                std::cerr << "Remote send failed: " << strerror(errno) << std::endl;
                close(sockfd);
                sockfd = -1;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// 写入日志到文件
void Logger::writeToFile(const std::string& message) {
    outFile << message << std::endl;
    checkFileSize(); // 检查文件大小并触发日志滚动
}

// 写入日志到远程服务器
void Logger::writeToRemote(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex);
    remoteQueue.push(message);
}

// 写入日志到 syslog
void Logger::writeToSyslog(LogLevel_en level, const std::string& message) {
    int priority = LOG_INFO;
    switch (level) {
        case DEBUG: priority = LOG_DEBUG; break;
        case INFO: priority = LOG_INFO; break;
        case WARNING: priority = LOG_WARNING; break;
        case ERROR: priority = LOG_ERR; break;
        case FATAL: priority = LOG_CRIT; break;
    }
    syslog(priority, "%s", message.c_str()); // 写入 syslog
}


void Logger::rotateLogs() {
    std::lock_guard<std::mutex> lock(mutex);
    outFile.close();  // 关闭当前日志文件

    // 删除最旧的文件（如果存在）
    fs::path oldestFile = logPath / (logName + "_" + std::to_string(maxFileCount) + ".log");
    if (fs::exists(oldestFile)) {
        fs::remove(oldestFile);
    }

    // 向后移动其他文件
    for (int i = maxFileCount - 1; i >= 1; --i) {
        fs::path src = logPath / (logName + "_" + std::to_string(i) + ".log");
        if (fs::exists(src)) {
            fs::path dest = logPath / (logName + "_" + std::to_string(i + 1) + ".log");
            fs::rename(src, dest);
        }
    }

    // 重命名当前文件为下一个编号的文件
    fs::path newFilePath = logPath / (logName + "_2.log");
    if (fs::exists(currentFilePath)) {
        fs::rename(currentFilePath, newFilePath);
    }

    // 创建新的当前日志文件
    currentFilePath = logPath / (logName + "_1.log");
    outFile.open(currentFilePath, std::ios::app);
    if (!outFile) {
        throw std::runtime_error("Failed to open new log file: " + currentFilePath.string());
    }

    std::cerr << "Log rotation completed. New log file created: " << currentFilePath << std::endl;
}

// 压缩文件
void Logger::compressFile(const fs::path& filePath) {
    std::unique_lock<std::mutex> lock(compressMutex);
    compressQueue.push([this, filePath]() {
        // 使用文件锁确保独占访问
        int fd = open(filePath.c_str(), O_RDWR);
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            std::ifstream inFile(filePath, std::ios::binary);
            if (!inFile) {
                std::cerr << "Failed to open file for compression: " << filePath << std::endl;
                return;
            }

            fs::path compressedFilePath = filePath.string() + ".gz";
            gzFile outFile = gzopen(compressedFilePath.c_str(), "wb");
            if (!outFile) {
                std::cerr << "Failed to open compressed file: " << compressedFilePath << std::endl;
                return;
            }
            char buffer[1024];
            while (inFile.read(buffer, sizeof(buffer))) {
                gzwrite(outFile, buffer, inFile.gcount());
            }
            gzclose(outFile);
            inFile.close();

            // 删除原始文件
            fs::remove(filePath);

            // 管理压缩文件列表
            std::lock_guard<std::mutex> lock(mutex);
            compressedFiles.push_back(compressedFilePath);
            if (compressedFiles.size() > maxCompressedFiles) {
                fs::remove(compressedFiles.front());
                compressedFiles.erase(compressedFiles.begin());
            }
            // 检查压缩文件大小
            if (fs::file_size(compressedFilePath) > maxCompressedFileSize) {
                fs::remove(compressedFilePath);
            }
            flock(fd, LOCK_UN);
        }
        close(fd);
    });
    compressCV.notify_one();
}

// 获取当前时间字符串
std::string Logger::getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
    
    // 添加时间精度部分
    auto duration = now.time_since_epoch();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);
    duration -= sec;

    switch (timePrecision) {
        case SECONDS:
            break;
        case MILLISECONDS: {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            oss << "." << std::setw(3) << std::setfill('0') << ms.count();
            break;
        }
        case MICROSECONDS: {
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration);
            oss << "." << std::setw(6) << std::setfill('0') << us.count();
            break;
        }
        case NANOSECONDS: {
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            oss << "." << std::setw(9) << std::setfill('0') << ns.count();
            break;
        }
    }
    return oss.str();
}

// 其他成员函数实现
void Logger::setLogPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    logPath = path;
    checkAndCreateLogDirectory();
    currentFilePath = logPath / (logName + "_1.log");

    outFile.close();
    outFile.open(currentFilePath, std::ios::app);
    if (!outFile) {
        throw std::runtime_error("Failed to reopen log file: " + currentFilePath.string());
    }
}

void Logger::setMaxFileSize(size_t maxFileSize) {
    std::lock_guard<std::mutex> lock(mutex);
    this->maxFileSize = maxFileSize;
}

void Logger::setMaxFileCount(size_t maxFileCount) {
    std::lock_guard<std::mutex> lock(mutex);
    if (maxFileCount < 1) throw std::invalid_argument("Max file count must be ≥1");
    this->maxFileCount = maxFileCount;
}

void Logger::setLogName(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    logName = name;
    currentFilePath = logPath / (logName + "_1.log");

    outFile.close();
    outFile.open(currentFilePath, std::ios::app);
    if (!outFile) {
        throw std::runtime_error("Failed to reopen log file: " + currentFilePath.string());
    }
}

void Logger::enableLogLevel(LogLevel_en level, bool enable) {
    std::lock_guard<std::mutex> lock(mutex);
    logLevelEnabled.set(level - 1, enable);
}

void Logger::enableLogLevelAbove(LogLevel_en level, bool enable) {
    std::lock_guard<std::mutex> lock(mutex);
    for (int i = level - 1; i < 5; ++i) {
        logLevelEnabled.set(i, enable);
    }
}

void Logger::setOutputToConsole(bool enable) {
    outputToConsole = enable;
}

void Logger::enableLogCompression(bool enable) {
    compressLogs = enable;
}

void Logger::setMaxCompressedFiles(size_t maxCompressedFiles) {
    this->maxCompressedFiles = maxCompressedFiles;
}

void Logger::setMaxCompressedFileSize(size_t maxCompressedFileSize) {
    this->maxCompressedFileSize = maxCompressedFileSize;
}

void Logger::setJsonFormat(bool enable) {
    jsonFormat = enable;
}

void Logger::enableRemoteLogging(const std::string& remoteIp, uint16_t remotePort) {
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, remoteIp.c_str(), &(sa.sin_addr)) == 0) {
        throw std::invalid_argument("Invalid IPv4 address: " + remoteIp);
    }
    this->remoteIp = remoteIp;
    this->remotePort = remotePort;
}

void Logger::enableSyslog(const std::string& ident, int facility, int syslogLevel) {
    if (!syslogInitialized) {
        openlog(ident.c_str(), LOG_PID | LOG_NDELAY, facility);
        syslogInitialized = true;
    }
    syslogIdent = ident;
    syslogFacility = facility;
    this->syslogLevel = syslogLevel;
}

void Logger::setMaxQueueSize(size_t size) {
    maxQueueSize.store(size);
}

void Logger::setTimePrecision(TimePrecision precision) {
    timePrecision = precision;
}

std::string Logger::getLogLevelString(LogLevel_en level) {
    switch (level) {
        case DEBUG:   return "DEBUG";
        case INFO:    return "INFO";
        case WARNING: return "WARNING";
        case ERROR:   return "ERROR";
        case FATAL:   return "FATAL";
        default:      return "UNKNOWN";
    }
}

void Logger::checkFileSize() {
    std::error_code ec;
    size_t fileSize = fs::file_size(currentFilePath, ec);
    if (!ec && fileSize >= maxFileSize) {
        if (fileSize >= maxFileSize) {
            rotateLogs();
            if (compressLogs) {
                compressFile(currentFilePath);
            }
        }
    }
}
