#!/bin/sh

ulimit -c 1024000
killall vfs_master -9

sleep 2;

dir=`dirname $0`
cd $dir
./vfs_master
