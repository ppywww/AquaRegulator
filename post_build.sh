#!/bin/sh
#post_build.sh 是一个构建后处理脚本，主要用于处理 HPSocket 库的依赖。它的核心功能是：

1. 库文件管理 ：检测、复制和链接 HPSocket 库文件
2. 环境配置 ：设置 LD_LIBRARY_PATH 环境变量
3. 错误处理 ：提供清晰的错误信息和状态反馈
filename=$(readlink -f "$0")
dir=$(dirname "$filename")

set -e
printf "\033[32mhello from post build script! \033[0m\n"

echo "export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${PWD}"

if [ -e "../build/libhpsocket.so" ]; then
    printf "\033[32mCreating symlink for libhpsocket.so.6 \033[0m\n"
    exit 0
fi

if [ -e "../3rdparty/linux/hp-socket-6.0.7-lib-linux/lib/hpsocket/x64/libhpsocket.so" ]; then
    cp ../3rdparty/linux/hp-socket-6.0.7-lib-linux/lib/hpsocket/x64/libhpsocket.so ../build
    ln -s ${dir}/../build/libhpsocket.so ${dir}/../build/libhpsocket.so.6
    printf "\033[32mCopied libhpsocket.so to AquaRegS directory. \033[0m\n"
else
    printf "\033[31mlibhpsocket.so not found! \033[0m\n"
    exit 1
fi