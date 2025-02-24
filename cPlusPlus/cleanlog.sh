#!/bin/bash

# 删除当前目录下所有以 .log 结尾的文件
rm -f logs/*.log || echo "No .log files to delete"