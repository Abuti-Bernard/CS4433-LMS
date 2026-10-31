#!/usr/bin/env bash
# Builds LMS without CMake. Run from the lms folder in the MSYS2 UCRT64 (or Linux) shell.
set -e
g++ -std=c++17 -O2 -Wall -Wextra -Isrc src/*.cpp -o lms -lsqlite3 -lssl -lcrypto
echo "Built ./lms  (on Windows: ./lms.exe)"
