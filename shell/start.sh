#!/bin/bash

./shell/build.sh

if [ $? -ne 0 ]; then
    echo 'Compilation failed'
else
    build/server 3000
    wait
fi