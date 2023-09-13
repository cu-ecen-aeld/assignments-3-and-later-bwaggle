#!/bin/sh

clear;
make
sudo ./aesdchar_unload
sudo ./aesdchar_load
strace -o /tmp/strace-aesdchar.txt -f /home/bwaggle/Assignments/assignment-autotest/test/assignment8/drivertest.sh