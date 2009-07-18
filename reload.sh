#!/bin/sh

rmmod etherip
insmod etherip.ko
./ethiptunnel -a -d 192.168.1.1 -n ethtest
ifconfig ethtest 192.168.2.1 up
