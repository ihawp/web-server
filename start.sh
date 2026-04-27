#!/bin/bash

gcc -iquote ./headers main.c includes/*.c -o build/server 

if [ $? -ne 0 ]; then
    echo 'Compilation failed'
else
    build/server 3001
    wait
fi