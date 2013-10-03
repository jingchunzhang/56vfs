#!/bin/sh

curtime=`date '+%Y%m%d%H%M%S'`

tar -czvf /diska/vfs/path/tmpdir/vfslog_$curtime.tar.gz /diska/vfs/log/* /diska/vfs/path/vfs*
rm -rf  /diska/vfs/log/*;
rm -f /diska/vfs/path/vfs*
