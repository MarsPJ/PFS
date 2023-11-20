#!/bin/bash

# 创建30个目录
for i in {1..30}
do
    dirname="directory$i"
    mkdir -p "$dirname"
done

echo "生成30个目录完成"
