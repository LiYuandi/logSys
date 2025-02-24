#include "simple_file_system.h"
#include <iostream>

int main() {

    // 添加循环来测试日志打印是否生成多个文件
    for (int i = 0; i < 5; ++i) {
        std::string log_message = "Log message number " + std::to_string(i);
        log_info(log_message.c_str());
    }

}