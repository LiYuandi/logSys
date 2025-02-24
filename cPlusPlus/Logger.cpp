#include "Logger.h"


Logger::Logger(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount)
    : logPath(path), logName(name), maxFileSize(maxFileSize), maxFileCount(maxFileCount)
{
    fs::create_directories(logPath);
    currentFilePath = logPath / (logName + ".log");

    outFile.open(currentFilePath, std::ios::app);
    if (!outFile)
    {
        throw std::runtime_error("Failed to open log file: " + currentFilePath.string());
    }
}

Logger& Logger::getInstance(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount)
{
    static Logger instance(path, name, maxFileSize, maxFileCount);
    return instance;
}

// void Logger::log(LogLevel_en level, const std::string& format, const char* file, int line, ...)
// {
//     std::lock_guard<std::mutex> lock(mutex);

//     // 使用 va_list 来获取可变参数
//     va_list args;
//     va_start(args, line);  // line 是最后一个命名参数
//     char buffer[1024];
//     vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
//     va_end(args);

//     std::string message(buffer);

//     outFile << "[" << getCurrentTimeString() << "]"
//             << "[" << getLogLevelString(level) << "]"
//             << "[" << file << ":" << line << "] "
//             << message << std::endl;

//     checkFileSize();
// }

void Logger::log(LogLevel_en level, const std::string& format, const char* file, int line, ...)
{
    std::lock_guard<std::mutex> lock(mutex);

    // 如果没有额外参数，可以直接处理格式化字符串
    if (format.empty()) {
        outFile << "[" << getCurrentTimeString() << "]"
                << "[" << getLogLevelString(level) << "]"
                << "[" << file << ":" << line << "] "
                << format << std::endl;
        return;
    }

    va_list args;
    va_start(args, line);  // line 是最后一个命名参数
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    va_end(args);

    std::string message(buffer);

    outFile << "[" << getCurrentTimeString() << "]"
            << "[" << getLogLevelString(level) << "]"
            << "[" << file << ":" << line << "] "
            << message << std::endl;

    checkFileSize();
}

void Logger::log(LogLevel_en level, const std::string& format, ...)
{
    std::lock_guard<std::mutex> lock(mutex);

    // 初始化 va_list
    va_list args;
    va_start(args, format);

    // 格式化日志消息
    char buffer[1024];  // 足够大以容纳格式化的字符串
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);

    // 结束 va_list 使用
    va_end(args);

    std::string message(buffer);
    std::string timestamp = getCurrentTimeString();

    outFile << "[" << timestamp << "]"
            << "[" << logLevelMap[level] << "]"
            << "[" << __FILE__ << ":" << __LINE__ << "] "
            << message << std::endl;

    checkFileSize();
}

std::string Logger::getLogLevelString(LogLevel_en level)
{
    auto it = logLevelMap.find(level);
    return it != logLevelMap.end() ? it->second : "UNKNOWN";
}

void Logger::checkFileSize()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (fs::file_size(currentFilePath) >= maxFileSize)
    {
        rotateLogs();
    }
}

void Logger::rotateLogs()
{
    std::lock_guard<std::mutex> lock(mutex);
    std::queue<fs::path> logFiles;

    for (const auto& entry : fs::directory_iterator(logPath))
    {
        if (fs::is_regular_file(entry) && entry.path().filename().string().find(logName) != std::string::npos)
        {
            logFiles.push(entry.path());
        }
    }


    


    while (logFiles.size() >= maxFileCount)
    {
        fs::remove(logFiles.front());
        logFiles.pop();
    }

    int newFileIndex = 1;
    fs::path newFilePath;
    do
    {
        newFilePath = logPath / (logName + "_" + std::to_string(newFileIndex) + ".log");
        newFileIndex++;
    } while (fs::exists(newFilePath));

    fs::rename(currentFilePath, newFilePath);
    currentFilePath = logPath / (logName + ".log");

    outFile.close();
    outFile.open(currentFilePath, std::ios::app);
    if (!outFile)
    {
        throw std::runtime_error("Failed to reopen log file after rotation: " + currentFilePath.string());
    }
}

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