#!/bin/bash

gcc -iquote ./src/include src/*.c -o build/server 

i=0;

if [ $? -ne 0 ]; then
    echo 'Compilation failed'
else
    build/server 3010
    wait
fi