#!/usr/bin/env bash

CFLAGS="-Wall -Wextra -g"
gcc -c caseconvert.c $CFLAGS -fPIC `pkg-config --cflags geany` -o caseconvert.o
gcc -c caseconvert-ui.c $CFLAGS -fPIC `pkg-config --cflags geany` -o caseconvert-ui.o
gcc caseconvert-ui.o caseconvert.o -g -o caseconvert.so -shared `pkg-config --libs geany`
