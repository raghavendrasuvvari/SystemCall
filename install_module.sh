#!/bin/sh
set -x
lsmod
rmmod sys_xconcat
insmod sys_xconcat.ko
lsmod
