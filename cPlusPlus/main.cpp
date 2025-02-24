#include<iostream>

#include<LOG.h>

int main() {
    LOG_INIT("./", "log", 1024 * 1024, 10);
    int i = 0;
    while (i < 1*5*1024)
    {
        LOG(INFO, "Hello World %d" ,i);
        i++;
    }
    

    LOG(INFO, "Hello World");
    return 0;
}