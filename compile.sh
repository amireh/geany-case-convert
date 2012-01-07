#!/usr/bin/env bash

gcc -c caseconvert.c -g -fPIC `pkg-config --cflags geany` -o caseconvert.o
gcc -c caseconvert-ui.c -g -fPIC `pkg-config --cflags geany` -o caseconvert-ui.o
gcc caseconvert-ui.o caseconvert.o -g -o caseconvert.so -shared `pkg-config --libs geany`
