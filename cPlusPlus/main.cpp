#include<iostream>
#include <unistd.h>

#include"LOG.h"

int main() {
    LOG_INIT("logs", "log",  1024, 5);
    int i = 0;
    while (i < 1*1024*1024)
    {
        LOG(INFO, "Hello World %d" ,i);
        i++;
        usleep(10);
    }
    
    return 0;
}

