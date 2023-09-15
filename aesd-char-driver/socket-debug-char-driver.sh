#!/bin/sh

clear;
make
sudo ./aesdchar_unload
sudo ./aesdchar_load
# pkill aesdsocket
# /home/bwaggle/Assignments/server/make
# /home/bwaggle/Assignments/server/aesdsocket
# /home/bwaggle/Assignments/assignment-autotest/test/assignment8/sockettest.sh
# strace -o /tmp/strace-aesdchar.txt -f /home/bwaggle/Assignments/assignment-autotest/test/assignment8/sockettest.sh