#include<iostream>
#include <unistd.h>
#include "Logger3.h"

int main() {
    // 在 getInstance 中显式传递日志名称 "aplog"
    Logger& logger = Logger::getInstance("/tmp/logs", "aplog", 10 * 1024, 3);
    
    // 基本配置
    logger.setLogPath("logs");
    logger.setMaxFileSize(1*1024* 1024); // 10MB
    logger.setMaxFileCount(5);

    // 高级功能
    logger.setTimePrecision(MILLISECONDS);
    logger.enableLogCompression(false);
    logger.setMaxCompressedFiles(20);
    // logger.enableRemoteLogging("192.168.1.100", 514);
    logger.setOutputToConsole(true);

    // 记录日志
    // logger.log(INFO, "System started. Version: %s", __FILE__, __LINE__, "1.4.2");
    // logger.log(DEBUG, "Sensor value: %.2f", __FILE__, __LINE__, 3.14159);

    // 记录大量日志以触发滚动
    for (int i = 0; i < 20000; ++i) {
        logger.log(INFO, "Test log entry %d", __FILE__, __LINE__, i);
        // usleep(10);
    }

    return 0;
}
