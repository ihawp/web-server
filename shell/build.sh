#!/bin/bash

gcc -iquote ./src/include src/*.c -o build/server -lpthread