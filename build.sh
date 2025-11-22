#!/bin/bash

docker run --rm \
  -v $PWD:/project \
  -w /project \
  --device=/dev/ttyUSB0 \
  espressif/idf:release-v5.5 \
  idf.py -p /dev/ttyUSB0 build flash
