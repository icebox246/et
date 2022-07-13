#!/bin/sh

set -xe

gcc et.c $(pkg-config ncurses --libs --cflags) -o et
