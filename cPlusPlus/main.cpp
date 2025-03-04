#include<iostream>
#include <unistd.h>
#include "Logger2.h"

int main() {
    // 获取Logger2实例
    Logger& logger = Logger::getInstance("logs", "app_log", 1024 * 1024, 5);

    // 启用日志压缩
    logger.enableLogCompression(true);
    logger.setMaxCompressedFiles(10);
    logger.setMaxCompressedFileSize(1024 * 1024);

    // 设置日志输出到终端
    logger.setOutputToConsole(true);

    logger.setJsonFormat(true);

    // 设置日志输出到远程服务器（TCP）
    // logger.enableRemoteLogging("127.0.0.1", 514);

    // 设置日志输出到syslog
    logger.enableSyslog("app_name", LOG_USER, LOG_DEBUG);

    // 记录不同等级的日志
    logger.log(DEBUG, "This is a debug message from %s at line %d", __FILE__, __LINE__, "123", 1);
    logger.log(INFO, "This is an info message from %s at line %d", __FILE__, __LINE__, "123", 1);
    logger.log(WARNING, "This is a warning message from %s at line %d", __FILE__, __LINE__, "123", 1);
    logger.log(ERROR, "This is an error message from %s at line %d", __FILE__, __LINE__, "123", 1);
    logger.log(FATAL, "This is a fatal message from %s at line %d", __FILE__, __LINE__, "123", 1);

    return 0;
}
