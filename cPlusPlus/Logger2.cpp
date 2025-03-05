#if 0

#include "Logger2.h"

// 构造函数
Logger::Logger(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount)
    : logPath(path), logName(name), maxFileSize(maxFileSize), maxFileCount(maxFileCount), running(true)
{
    if (maxFileCount < 1) 
    {
        throw std::invalid_argument("maxFileCount must be at least 1");
    }

    checkAndCreateLogDirectory(); // 检查并创建日志目录
    currentFilePath = logPath / (logName + "_1.log"); // 当前日志文件路径

    outFile.open(currentFilePath, std::ios::app); // 打开日志文件
    if (!outFile)
    {
        throw std::runtime_error("Failed to open log file: " + currentFilePath.string());
    }

    writeThread = std::thread(&Logger::writeThreadFunc, this); // 启动日志写入线程
}

// 析构函数
Logger::~Logger()
{
    running = false;
    cv.notify_all();
    if (writeThread.joinable()) {
        writeThread.join();
    }
    // 处理剩余日志
    while (!logQueue.empty()) {
        writeToFile(logQueue.front());
        logQueue.pop();
    }

    if (sockfd != -1)
    {
        close(sockfd); // 关闭 TCP 套接字
    }

    if (useSyslog)
    {
        closelog(); // 关闭 syslog
    }
}

// 获取单例实例
Logger& Logger::getInstance(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount)
{
    static Logger instance(path, name, maxFileSize, maxFileCount);
    return instance;
}

// 检查并创建日志目录
void Logger::checkAndCreateLogDirectory()
{
    std::error_code ec;
    if (!fs::exists(logPath))
    {
        if (!fs::create_directories(logPath, ec))
        {
            throw std::runtime_error("Failed to create log directory: " + logPath.string());
        }
    }
    else if (ec)
    {
        throw std::runtime_error("Error accessing log directory: " + logPath.string());
    }
}

// 日志记录方法（带文件名和行号）
void Logger::log(LogLevel_en level, const std::string& format, const char* file, int line, ...)
{
    if (!(logLevelEnabled.test(level - 1))) return; // 如果该日志等级未启用，直接返回

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
    if (jsonFormat) // JSON 格式日志
    {
        logEntry << "{"
                 << "\"timestamp\":\"" << getCurrentTimeString() << "\","
                 << "\"level\":\"" << getLogLevelString(level) << "\","
                 << "\"file\":\"" << (file ? file : "unknown") << "\","
                 << "\"line\":" << line << ","
                 << "\"message\":\"" << message << "\""
                 << "}";
    }
    else // 纯文本格式日志
    {
        logEntry << "[" << getCurrentTimeString() << "]"
                 << "[" << getLogLevelString(level) << "]"
                 << "[" << (file ? file : "unknown") << ":" << line << "] "
                 << message;
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (logQueue.size() >= 1000) { // 日志队列大小限制
        return;
    }
    logQueue.push(logEntry.str()); // 将日志消息加入队列
    cv.notify_one(); // 通知写线程

    if (outputToConsole) // 输出到终端
    {
        std::cout << logEntry.str() << std::endl;
    }

    if (useSyslog && syslogLevel <= level) // 输出到 syslog
    {
        writeToSyslog(level, message);
    }

    if (!remoteIp.empty() && remotePort != 0) // 输出到远程服务器
    {
        writeToRemote(logEntry.str());
    }
}

// 日志写入线程函数
void Logger::writeThreadFunc()
{
    while (running)
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !logQueue.empty() || !running; }); // 等待日志消息

        while (!logQueue.empty())
        {
            std::string message = logQueue.front();
            logQueue.pop();
            lock.unlock();

            writeToFile(message); // 写入日志到文件

            lock.lock();
        }
    }
}

// 写入日志到文件
void Logger::writeToFile(const std::string& message)
{
    outFile << message << std::endl;
    checkFileSize(); // 检查文件大小并触发日志滚动
}

// 写入日志到远程服务器
void Logger::writeToRemote(const std::string& message)
{
    if (sockfd == -1)
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建 TCP 套接字
        if (sockfd < 0)
        {
            std::cerr << "Failed to create socket for remote logging" << std::endl;
            return;
        }

        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(remotePort);
        inet_pton(AF_INET, remoteIp.c_str(), &serverAddr.sin_addr);

        if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
        {
            std::cerr << "Failed to connect to remote server" << std::endl;
            close(sockfd);
            sockfd = -1;
            return;
        }
    }

    send(sockfd, message.c_str(), message.size(), 0); // 发送日志消息
}

// 写入日志到 syslog
void Logger::writeToSyslog(LogLevel_en level, const std::string& message)
{
    int priority = LOG_INFO;
    switch (level)
    {
        case DEBUG: priority = LOG_DEBUG; break;
        case INFO: priority = LOG_INFO; break;
        case WARNING: priority = LOG_WARNING; break;
        case ERROR: priority = LOG_ERR; break;
        case FATAL: priority = LOG_CRIT; break;
    }
    syslog(priority, "%s", message.c_str()); // 写入 syslog
}

// 日志滚动
void Logger::rotateLogs() {
    std::lock_guard<std::mutex> lock(mutex);
    outFile.close();

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

    // 重命名当前文件为1.log
    fs::rename(currentFilePath, logPath / (logName + "_1.log"));

    // 重新打开日志文件
    outFile.open(currentFilePath, std::ios::app);
    if (!outFile) {
        throw std::runtime_error("Failed to reopen log file after rotation");
    }
}

// 压缩文件
void Logger::compressFile(const fs::path& filePath)
{
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Failed to open file for compression: " << filePath << std::endl;
        return;
    }

    fs::path compressedFilePath = filePath.string() + ".gz";
    gzFile outFile = gzopen(compressedFilePath.c_str(), "wb");
    if (!outFile)
    {
        std::cerr << "Failed to open compressed file: " << compressedFilePath << std::endl;
        return;
    }

    char buffer[1024];
    while (inFile.read(buffer, sizeof(buffer)))
    {
        gzwrite(outFile, buffer, inFile.gcount());
    }
    gzclose(outFile);
    inFile.close();

    std::lock_guard<std::mutex> lock(mutex);
    compressedFiles.push_back(compressedFilePath);
    if (compressedFiles.size() > maxCompressedFiles)
    {
        fs::remove(compressedFiles.front());
        compressedFiles.erase(compressedFiles.begin());
    }
    else if (fs::file_size(compressedFilePath) > maxCompressedFileSize)
    {
        fs::remove(compressedFilePath);
    }
}

// 获取当前时间字符串
std::string Logger::getCurrentTimeString()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << now_ms.count();
    return oss.str();
}

// 设置日志存储路径
void Logger::setLogPath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex);
    logPath = path;
    checkAndCreateLogDirectory(); // 确保新路径存在
    currentFilePath = logPath / (logName + "_1.log");

    outFile.close(); // 关闭旧文件
    outFile.open(currentFilePath, std::ios::app);
    if (!outFile)
    {
        throw std::runtime_error("Failed to reopen log file after changing path: " + currentFilePath.string());
    }
}

// 设置单个日志文件最大大小
void Logger::setMaxFileSize(size_t maxFileSize)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->maxFileSize = maxFileSize;
}

// 设置最大日志文件数量
void Logger::setMaxFileCount(size_t maxFileCount)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (maxFileCount < 1)
    {
        throw std::invalid_argument("Max file count must be at least 1");
    }
    this->maxFileCount = maxFileCount;
}

// 设置日志文件名
void Logger::setLogName(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex);
    logName = name;
    currentFilePath = logPath / (logName + "_1.log");

    outFile.close(); // 关闭旧文件
    outFile.open(currentFilePath, std::ios::app);
    if (!outFile)
    {
        throw std::runtime_error("Failed to reopen log file after changing name: " + currentFilePath.string());
    }
}

// 启用/禁用特定日志等级
void Logger::enableLogLevel(LogLevel_en level, bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    logLevelEnabled.set(level - 1, enable);
}

// 启用/禁用某个等级及以上的所有日志
void Logger::enableLogLevelAbove(LogLevel_en level, bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    for (int i = level - 1; i < 5; ++i)
    {
        logLevelEnabled.set(i, enable);
    }
}

// 设置是否输出到终端
void Logger::setOutputToConsole(bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    outputToConsole = enable;
}

// 启用/禁用日志压缩
void Logger::enableLogCompression(bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    compressLogs = enable;
}

// 设置 JSON 格式日志开关
void Logger::setJsonFormat(bool enable)
{
    std::lock_guard<std::mutex> lock(mutex);
    jsonFormat = enable;
}

// 启用远程日志（TCP）
void Logger::enableRemoteLogging(const std::string& remoteIp, uint16_t remotePort)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->remoteIp = remoteIp;
    this->remotePort = remotePort;

    // 验证 IP 地址格式
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, remoteIp.c_str(), &(sa.sin_addr)) == 0)
    {
        throw std::invalid_argument("Invalid IPv4 address: " + remoteIp);
    }
}

// 启用 syslog 支持
void Logger::enableSyslog(const std::string& ident, int facility, int syslogLevel)
{
    std::lock_guard<std::mutex> lock(mutex);
    useSyslog = true;
    syslogIdent = ident;
    syslogFacility = facility;
    this->syslogLevel = syslogLevel;
    openlog(ident.c_str(), LOG_PID | LOG_NDELAY, facility); // 打开 syslog 连接
}

// 获取日志等级对应的字符串
std::string Logger::getLogLevelString(LogLevel_en level)
{
    switch (level)
    {
        case DEBUG: return "DEBUG";
        case INFO: return "INFO";
        case WARNING: return "WARNING";
        case ERROR: return "ERROR";
        case FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// 检查文件大小并触发滚动
void Logger::checkFileSize()
{
    std::error_code ec;
    size_t fileSize = fs::file_size(currentFilePath, ec);
    if (!ec && fileSize >= maxFileSize)
    {
        rotateLogs(); // 触发日志滚动
        if (compressLogs)
        {
            compressFile(currentFilePath);
        }
    }
}

// 设置最大压缩文件数量
void Logger::setMaxCompressedFiles(size_t maxCompressedFiles)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->maxCompressedFiles = maxCompressedFiles;
}

// 设置单个压缩文件最大大小
void Logger::setMaxCompressedFileSize(size_t maxCompressedFileSize)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->maxCompressedFileSize = maxCompressedFileSize;
}

#endif