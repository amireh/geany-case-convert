#!/usr/bin/env bash

CFLAGS="-Wall -Wextra -ansi -pedantic -g -DVERBOSE"
#~ CFLAGS="-Wall -Wextra -ansi -pedantic -O2"
gcc -c caseconvert.c $CFLAGS -fPIC `pkg-config --cflags geany` -o caseconvert.o
gcc -c caseconvert_ui.c $CFLAGS -fPIC `pkg-config --cflags geany` -o caseconvert_ui.o
gcc -c caseconvert_types.c $CFLAGS -fPIC `pkg-config --cflags geany` -o caseconvert_types.o
gcc caseconvert_ui.o caseconvert_types.o caseconvert.o -g -o caseconvert.so -shared `pkg-config --libs geany`
