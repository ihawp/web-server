#!/bin/bash

gcc -iquote ./src/include src/*.c -o build/server

if [ $? -ne 0 ]; then
    echo 'Compilation failed'
else
    build/server 3000
    wait
fi