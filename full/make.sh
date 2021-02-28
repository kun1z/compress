#!/usr/bin/bash

# Copyright © 2021 by Brett Kuntz. All rights reserved.

clang compress.c -o compress.exe -pipe -pthread -Wall -Werror -Wfatal-errors -O3 -fomit-frame-pointer -march=native -mtune=native
clang decompress.c -o decompress.exe -pipe -pthread -Wall -Werror -Wfatal-errors -O3 -fomit-frame-pointer -march=native -mtune=native

# comment the above lines & uncomment the below lines on Linux/POSIX OS's as they will likely support -flto

#clang compress.c -o compress -pipe -pthread -Wall -Werror -Wfatal-errors -O3 -fomit-frame-pointer -flto -march=native -mtune=native
#clang decompress.c -o decompress -pipe -pthread -Wall -Werror -Wfatal-errors -O3 -fomit-frame-pointer -flto -march=native -mtune=native