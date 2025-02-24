#include "Logger.h"

Logger::Logger(const std::string& path, const std::string& name, size_t maxFileSize, size_t maxFileCount)
    : logPath(path), logName(name), maxFileSize(maxFileSize), maxFileCount(maxFileCount)
{
    checkAndCreateLogDirectory();  // 确保路径存在
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

void Logger::log(LogLevel_en level, const std::string& format, const char* file, int line, ...)
{
    std::lock_guard<std::mutex> lock(mutex);

    // 如果文件流无效，则重新打开文件
    if (!outFile.is_open()) {
        outFile.open(currentFilePath, std::ios::app);
        if (!outFile) {
            std::cerr << "Failed to open log file for writing: " << currentFilePath << std::endl;
            return;  // 或者可以选择抛出异常
        }
    }

    va_list args;
    va_start(args, line);

    // 使用 std::vector 动态分配足够大的缓冲区
    std::vector<char> buffer(1024);  // 初始大小 1024 字节，之后会根据需要扩展

    int len = vsnprintf(buffer.data(), buffer.size(), format.c_str(), args);
    if (static_cast<std::vector<char>::size_type>(len) >= buffer.size()) {  // 如果格式化后内容超出了缓冲区大小
        buffer.resize(len + 1);  // 动态扩展缓冲区
        vsnprintf(buffer.data(), buffer.size(), format.c_str(), args);  // 重新格式化
    }

    va_end(args);

    std::string message(buffer.data());

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

    // 使用 std::vector 动态分配足够大的缓冲区
    std::vector<char> buffer(1024);

    int len = vsnprintf(buffer.data(), buffer.size(), format.c_str(), args);
    if (static_cast<std::vector<char>::size_type>(len) >= buffer.size()) {  // 如果格式化后内容超出了缓冲区大小
        buffer.resize(len + 1);  // 动态扩展缓冲区
        vsnprintf(buffer.data(), buffer.size(), format.c_str(), args);  // 重新格式化
    }

    va_end(args);

    std::string message(buffer.data());
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
