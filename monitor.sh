#!/bin/bash

# 串口监控脚本 - 需要交互式终端
docker run --rm -it \
  -v $PWD:/project \
  -w /project \
  --device=/dev/ttyUSB0 \
  espressif/idf:release-v5.5 \
  idf.py -p /dev/ttyUSB0 monitor
