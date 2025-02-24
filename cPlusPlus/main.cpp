#include "Logger.h"
#include <iostream>

int main() {
    // 创建日志系统实例
    Logger::getInstance("logs", "app.log", 1024 * 1024, 5);

    // 测试不同等级的日志记录
    // LOG(DEBUG, "This is a debug message.");
    // LOG(INFO, "This is an info message.");
    // LOG(WARNING, "This is a warning message.");
    // LOG(ERROR, "This is an error message.");
    LOG(FATAL, "This is a fatal message.");

    LOG(INFO, "This is an info message with number: %d", 42);
LOG(ERROR, "An error occurred in function: %s", "main");

    return 0;
}