#!/bin/bash

valgrind -s -v --leak-check=full --show-leak-kinds=all ./build/server 3000 > valgrind.txt 2>&1