#!/bin/bash

./shell/build.sh

valgrind -s -v --leak-check=full --track-origins=yes --show-leak-kinds=all ./build/server 3000 > valgrind.txt 2>&1