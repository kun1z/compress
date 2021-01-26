#!/usr/bin/bash

# Copyright © 2021 by Brett Kuntz. All rights reserved.

#clang compress.c -o compute -pthread -lm -Wall -Werror -Wfatal-errors -pipe -O3 -flto -fomit-frame-pointer -march=native -mtune=native
clang compress.c -o compute.exe -pthread -lm -Wall -Werror -Wfatal-errors -pipe -O3 -fomit-frame-pointer -march=native -mtune=native