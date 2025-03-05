# Logger2 类使用手册

## 概述
`Logger2` 类是一个功能强大的日志记录工具，支持多种日志输出方式，包括文件、终端、远程服务器和syslog。它还支持日志压缩、JSON格式日志和日志等级控制等功能。

## 类方法说明


### 获取单例实例

根据需求，我将生成一个Markdown文件，详细描述`Logger2.h`和`Logger2.cpp`中的`Logger`类的使用方法，并说明每个函数的使用方法。以下是生成的Markdown文件内容：



```markdown::/home/li/A001_gitItem/gitDepository/log/cPlusPlus/Logger2.md::bc9f3be0-6d35-4491-9433-16162aca099c
```

- **参数**:
  - `path`: 日志文件存储路径，默认为 `/tmp/logs`。
  - `name`: 日志文件名，默认为 `app_log`。
  - `maxFileSize`: 单个日志文件最大大小，默认为 1MB。
  - `maxFileCount`: 最大日志文件数量，默认为 5。
- **返回值**: `Logger` 类的单例实例。

### 禁止拷贝和赋值
```cpp
Logger(const Logger&) = delete;
Logger& operator=(const Logger&) = delete;
```
- **说明**: 禁止拷贝和赋值操作，确保 `Logger` 类的单例特性。

### 日志记录方法
```cpp
void log(LogLevel_en level, const std::string& format, const char* file, int line, ...);
```
- **参数**:
  - `level`: 日志等级，可以是 `DEBUG`, `INFO`, `WARNING`, `ERROR`, `FATAL`。
  - `format`: 日志格式字符串，支持格式化输出。
  - `file`: 文件名，通常使用 `__FILE__` 宏。
  - `line`: 行号，通常使用 `__LINE__` 宏。
  - `...`: 格式化参数。
- **说明**: 记录一条日志消息。

### 修改配置方法
```cpp
void setLogPath(const std::string& path);
void setMaxFileSize(size_t maxFileSize);
void setMaxFileCount(size_t maxFileCount);
void setLogName(const std::string& name);
```
- **说明**: 修改日志记录的配置参数。

### 打印等级控制方法
```cpp
void enableLogLevel(LogLevel_en level, bool enable);
void enableLogLevelAbove(LogLevel_en level, bool enable);
```
- **说明**: 启用或禁用特定日志等级或以上等级的日志记录。

### 控制是否输出到终端
```cpp
void setOutputToConsole(bool enable);
```
- **说明**: 启用或禁用日志输出到终端。

### 压缩日志开关
```cpp
void enableLogCompression(bool enable);
void setMaxCompressedFiles(size_t maxCompressedFiles);
void setMaxCompressedFileSize(size_t maxCompressedFileSize);
```
- **说明**: 启用或禁用日志压缩，并设置压缩文件的相关参数。

### JSON 格式日志开关
```cpp
void setJsonFormat(bool enable);
```
- **说明**: 启用或禁用 JSON 格式的日志记录。

### 远程日志配置
```cpp
void enableRemoteLogging(const std::string& remoteIp, uint16_t remotePort);
void enableSyslog(const std::string& ident, int facility, int syslogLevel);
```
- **说明**: 配置远程日志记录和 syslog 支持。

### 析构函数
```cpp
~Logger();
```
- **说明**: 析构函数，负责清理资源。

## 使用示例

### 基本使用
```cpp
#include "Logger2.h"

int main() {
    Logger& logger = Logger::getInstance("/var/log/myapp", "applog", 1024 * 1024, 10);
    logger.log(INFO, "Application started at %s", __FILE__, __LINE__, logger.getCurrentTimeString().c_str());
    logger.log(DEBUG, "Debug message at %d", __FILE__, __LINE__, 123);
    return 0;
}
```

### 配置日志记录
```cpp
logger.setOutputToConsole(true);
logger.enableLogLevelAbove(WARNING, true);
logger.enableLogCompression(true);
logger.setMaxCompressedFiles(5);
logger.setMaxCompressedFileSize(1024 * 1024 * 5);
logger.setJsonFormat(true);
logger.enableRemoteLogging("192.168.1.100", 514);
logger.enableSyslog("myapp", LOG_USER, LOG_DEBUG);
```

### 自定义日志路径和文件名
```cpp
logger.setLogPath("/home/user/logs");
logger.setLogName("custom_log");
```

通过以上方法，您可以灵活地配置和使用 `Logger2` 类来满足各种日志记录需求。
```

这个Markdown文件详细描述了`Logger2`类的使用方法和每个函数的使用说明，以及一些使用示例。