#!/bin/sh

cd $(dirname $0)
if make debug
then
    echo ""
    ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=suppr.txt ./build/kmp "$@"
fi

