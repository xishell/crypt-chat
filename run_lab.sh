#!/bin/bash

if ! pgrep -x "jtagd" >/dev/null; then
    echo "Starting jtagd..."
    jtagd --user-start
    sleep 2
else
    echo "jtagd is already running"
fi

echo "Running build/main.bin..."
dtekv-run build/main.bin
