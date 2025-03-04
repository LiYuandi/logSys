#include<iostream>
#include <unistd.h>

// #include"LOG.h"

#include"Logger1.h"
int main() {
    // LOG_INIT("logs", "log",  1024, 5);
    // int i = 0;
    // while (i < 1*1024*1024)
    // {
    //     // LOG(INFO, "Hello World %d" ,i);
    //     i++;
    //     usleep(10);
    // }

    Logger& logger = Logger::getInstance();

    // 禁用 DEBUG 等级日志
    logger.enableLogLevel(DEBUG, false);

    



    
    return 0;
}

