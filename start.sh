#!/bin/bash

gcc -iquote ./src/include src/*.c -o build/server 

i=0;

if [ $? -ne 0 ]; then
    echo 'Compilation failed'
else
    for ((i=3000; i<3009; i++)); do
        build/server $i
        if [$? -ne 1]; then
            break;
        fi
    done
    wait
fi